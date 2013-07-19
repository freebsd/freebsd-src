/*
 * Copyright (C) 1993-2001 by Darren Reed.
 * (C)opyright 1997 by Marc Boucher.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */

/* TODO: (MARCXXX)
	- ipl_init failure -> open ENODEV or whatever
	- prevent multiple LKM loads
	- surround access to ifnet structures by IFNET_LOCK()/IFNET_UNLOCK() ?
	- m != m1 problem
*/

#include <sys/types.h>
#include <sys/conf.h>
#ifdef IPFILTER_LKM
#include <sys/mload.h>
#endif
#include <sys/systm.h>
#include <sys/errno.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#ifdef IFF_DRVRLOCK /* IRIX6 */
#include <sys/hashing.h>
#include <netinet/in_var.h>
#endif
#include <sys/mbuf.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ipfilter.h>
#include "ipl.h"
#include "ip_compat.h"
#include "ip_fil.h"
#include "ip_nat.h"

/*#define IPFDEBUG 1*/

unsigned IPL_EXTERN(devflag) = D_MP;
#ifdef IPFILTER_LKM
char *IPL_EXTERN(mversion) = M_VERSION;
#endif

kmutex_t ipl_mutex, ipf_mutex, ipfi_mutex, ipf_rw;
kmutex_t ipf_frag, ipf_state, ipf_nat, ipf_natfrag, ipf_auth;

int     (*fr_checkp) __P((struct ip *, int, void *, int, mb_t **));

#ifdef IPFILTER_LKM
static int *ipff_addr = 0;
static int ipff_value;
static __psunsigned_t *ipfk_addr = 0;
static __psunsigned_t ipfk_code[4];
#endif

typedef	struct	nif	{
	struct	nif	*nf_next;
	struct ifnet	*nf_ifp;
#if IRIX < 605
	int     (*nf_output)(struct ifnet *, struct mbuf *, struct sockaddr *);
#else
	int     (*nf_output)(struct ifnet *, struct mbuf *, struct sockaddr *,
			     struct rtentry *);
#endif
	char	nf_name[IFNAMSIZ];
	int	nf_unit;
} nif_t;

static nif_t *nif_head = 0;
static int nif_interfaces = 0;
extern int in_interfaces;

extern ipnat_t *nat_list;

static int
#if IRIX < 605
ipl_if_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst)
#else
ipl_if_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	      struct rtentry *rt)
#endif
{
	nif_t *nif;

	MUTEX_ENTER(&ipfi_mutex); /* sets interrupt priority level to splhi */
	for (nif = nif_head; nif; nif = nif->nf_next)
		if (nif->nf_ifp == ifp)
			break;

	MUTEX_EXIT(&ipfi_mutex);
	if (!nif) {
		printf("IP Filter: ipl_if_output intf %x NOT FOUND\n", ifp);
		return ENETDOWN;
	}

#if	IPFDEBUG >= 4
	static unsigned int cnt = 0;
	if ((++cnt % 200) == 0)
		printf("IP Filter: ipl_if_output(ifp=0x%lx, m=0x%lx, dst=0x%lx), m_type=%d m_flags=0x%lx m_off=0x%lx\n", ifp, m, dst, m->m_type, (unsigned long)(m->m_flags), m->m_off);
#endif
	if (fr_checkp) {
		struct mbuf *m1 = m;
		struct ip *ip;
		int hlen;

		switch(m->m_type) {
		case MT_DATA:
			if (m->m_flags & M_BCAST) {
#if	IPFDEBUG >= 2
				printf("IP Filter: ipl_if_output: passing M_BCAST\n");
#endif
				break;
			}
			/* FALLTHROUGH */
		case MT_HEADER:
#if	IPFDEBUG >= 4
			if (!MBUF_IS_CLUSTER(m) && ((m->m_off < MMINOFF) || (m->m_off > MMAXOFF))) {
				printf("IP Filter: ipl_if_output: bad m_off m_type=%d m_flags=0x%lx m_off=0x%lx\n", m->m_type, (unsigned long)(m->m_flags), m->m_off);
				goto done;
			}
#endif
			if (m->m_len < sizeof(char)) {
				printf("IP Filter: ipl_if_output: mbuf block too small (m_len=%d) for IP vers+hlen, m_type=%d m_flags=0x%lx\n", m->m_len, m->m_type, (unsigned long)(m->m_flags));
				goto done;
			}
			ip = mtod(m, struct ip *);
			if (ip->ip_v != IPVERSION) {
#if	IPFDEBUG >= 4
				printf("IP Filter: ipl_if_output: bad ip_v m_type=%d m_flags=0x%lx m_off=0x%lx\n", m->m_type, (unsigned long)(m->m_flags), m->m_off);
#endif
				goto done;
			}

			hlen = ip->ip_hl << 2;
			if ((*fr_checkp)(ip, hlen, ifp, 1, &m1))
				return EHOSTUNREACH;

			if (!m1)
				return 0;

			m = m1;
			break;

		default:
			printf("IP Filter: ipl_if_output: bad m_type=%d m_flags=0x%lxm_off=0x%lx\n", m->m_type, (unsigned long)(m->m_flags), m->m_off);
			break;
		}
	}
done:
#if IRIX < 605
	return (*nif->nf_output)(ifp, m, dst);
#else
	return (*nif->nf_output)(ifp, m, dst, rt);
#endif
}

int
IPL_EXTERN(_kernel)(struct ifnet *rcvif, struct mbuf *m)
{
#if	IPFDEBUG >= 4
	static unsigned int cnt = 0;
	if ((++cnt % 200) == 0)
		printf("IP Filter: ipl_ipfilter_kernel(rcvif=0x%lx, m=0x%lx\n", rcvif, m);
#endif

	/*
	 * Check if we want to allow this packet to be processed.
	 * Consider it to be bad if not.
	 */
	if (fr_checkp) {
		struct mbuf *m1 = m;
		struct ip *ip;
		int hlen;

		if ((m->m_type != MT_DATA) && (m->m_type != MT_HEADER)) {
			printf("IP Filter: ipl_ipfilter_kernel: bad m_type=%d m_flags=0x%lx m_off=0x%lx\n", m->m_type, (unsigned long)(m->m_flags), m->m_off);
			return IPF_ACCEPTIT;
		}

#if	IPFDEBUG >= 4
		if (!MBUF_IS_CLUSTER(m) && ((m->m_off < MMINOFF) || (m->m_off > MMAXOFF))) {
			printf("IP Filter: ipl_ipfilter_kernel: bad m_off m_type=%d m_flags=0x%lx m_off=0x%lx\n", m->m_type, (unsigned long)(m->m_flags), m->m_off);
			return IPF_ACCEPTIT;
		}
#endif
		if (m->m_len < sizeof(char)) {
			printf("IP Filter: ipl_ipfilter_kernel: mbuf block too small (m_len=%d) for IP vers+hlen, m_type=%d m_flags=0x%lx\n", m->m_len, m->m_type, (unsigned long)(m->m_flags));
			return IPF_ACCEPTIT;
		}
		ip = mtod(m, struct ip *);
		if (ip->ip_v != IPVERSION) {
			printf("IP Filter: ipl_ipfilter_kernel: bad ip_v\n");
			m_freem(m);
			return IPF_DROPIT;
		}

		hlen = ip->ip_hl << 2;
		if ((*fr_checkp)(ip, hlen, rcvif, 0, &m1) || !m1)
			return IPF_DROPIT;
		if (m != m1)
			printf("IP Filter: ipl_ipfilter_kernel: m != m1\n");
	}

	return IPF_ACCEPTIT;
}

static int
ipfilterattach(void)
{
#ifdef IPFILTER_LKM
	__psunsigned_t *addr_ff, *addr_fk;

	st_findaddr("ipfilterflag", &addr_ff);
#if	IPFDEBUG >= 4
	printf("IP Filter: st_findaddr ipfilterflag=0x%lx\n", addr_ff);
#endif
	if (!addr_ff)
		return ESRCH;

	st_findaddr("ipfilter_kernel", &addr_fk);
#if	IPFDEBUG >= 4
	printf("IP Filter: st_findaddr ipfilter_kernel=0x%lx\n", addr_fk);
#endif
	if (!addr_fk)
		return ESRCH;

	MUTEX_ENTER(&ipfi_mutex); /* sets interrupt priority level to splhi */

	ipff_addr = (int *)addr_ff;
	
	ipff_value = *ipff_addr;
	*ipff_addr = 0;


	ipfk_addr = addr_fk;

	bcopy(ipfk_addr, ipfk_code,
		sizeof(ipfk_code));

	/* write a "li t4, ipl_ipfilter_kernel" instruction */
	ipfk_addr[0] = 0x3c0c0000 |
		       (((__psunsigned_t)IPL_EXTERN(_kernel) >> 16) & 0xffff);
	ipfk_addr[1] = 0x358c0000 |
		       ((__psunsigned_t)IPL_EXTERN(_kernel) & 0xffff);
	/* write a "jr t4" instruction" */
	ipfk_addr[2] = 0x01800008;

	/* write a "nop" instruction */
	ipfk_addr[3] = 0;

	icache_inval(ipfk_addr, sizeof(ipfk_code));

	*ipff_addr = 1; /* enable ipfilter_kernel */

	MUTEX_EXIT(&ipfi_mutex);
#else
	extern int ipfilterflag;

	ipfilterflag = 1;
#endif

	return 0;
}

/*
 * attach the packet filter to each non-loopback interface that is running
 */
static void
nifattach()
{
	struct ifnet *ifp;
	struct frentry *f;
	ipnat_t *np;
	nif_t *nif;

	MUTEX_ENTER(&ipfi_mutex); /* sets interrupt priority level to splhi */

	for (ifp = ifnet; ifp; ifp = ifp->if_next) {
		if ((!(ifp->if_flags & IFF_RUNNING)) ||
			(ifp->if_flags & IFF_LOOPBACK))
			continue;

		/*
		 * Look for entry already setup for this device
		 */
		for (nif = nif_head; nif; nif = nif->nf_next)
			if (nif->nf_ifp == ifp)
				break;
		if (nif)
			continue;

		if (ifp->if_output == ipl_if_output) {
			printf("IP Filter: ERROR INTF 0x%lx STILL ATTACHED\n",
				ifp);
			continue;
		}
#if	IPFDEBUG >= 4
		printf("IP Filter: nifattach nif %x opt %x\n",
		       ifp, ifp->if_output);
#endif
		KMALLOC(nif, nif_t *);
		if (!nif) {
			printf("IP Filter: malloc(%d) for nif_t failed\n",
			       sizeof(nif_t));
			continue;
		}

		nif->nf_ifp = ifp;
		strncpy(nif->nf_name, ifp->if_name, sizeof(nif->nf_name));
		nif->nf_name[sizeof(nif->nf_name) - 1] = '\0';
		nif->nf_unit = ifp->if_unit;

		nif->nf_next = nif_head;
		nif_head = nif;

		/*
		 * Activate any rules directly associated with this interface
		 */
		MUTEX_ENTER(&ipf_mutex);
		for (f = ipfilter[0][0]; f; f = f->fr_next) {
			if ((f->fr_ifa == (struct ifnet *)-1)) {
				if (f->fr_ifname[0] &&
				    (GETUNIT(f->fr_ifname, 4) == ifp))
					f->fr_ifa = ifp;
			}
		}
		for (f = ipfilter[1][0]; f; f = f->fr_next) {
			if ((f->fr_ifa == (struct ifnet *)-1)) {
				if (f->fr_ifname[0] &&
				    (GETUNIT(f->fr_ifname, 4) == ifp))
					f->fr_ifa = ifp;
			}
		}
		MUTEX_EXIT(&ipf_mutex);
		MUTEX_ENTER(&ipf_nat);
		for (np = nat_list; np; np = np->in_next) {
			if ((np->in_ifp == (void *)-1)) {
				if (np->in_ifname[0] &&
				    (GETUNIT(np->in_ifname, 4) == ifp))
					np->in_ifp = (void *)ifp;
			}
		}
		MUTEX_EXIT(&ipf_nat);

		nif->nf_output = ifp->if_output;
		ifp->if_output = ipl_if_output;

#if	IPFDEBUG >= 4
		printf("IP Filter: nifattach: ifp(%lx)->if_output FROM %lx TO %lx\n",
			ifp, nif->nf_output, ifp->if_output);
#endif

		printf("IP Filter: attach to [%s,%d]\n",
			nif->nf_name, ifp->if_unit);
	}
	if (!nif_head)
		printf("IP Filter: not attached to any interfaces\n");

	nif_interfaces = in_interfaces;

	MUTEX_EXIT(&ipfi_mutex);

	return;
}

/*
 * look for bad consistancies between the list of interfaces the filter knows
 * about and those which are currently configured.
 */
int
ipfsync(void)
{
	register struct frentry *f;
	register ipnat_t *np;
	register nif_t *nif, **qp;
	register struct ifnet *ifp;

	MUTEX_ENTER(&ipfi_mutex); /* sets interrupt priority level to splhi */
	for (qp = &nif_head; (nif = *qp); ) {
		for (ifp = ifnet; ifp; ifp = ifp->if_next)
			if ((nif->nf_ifp == ifp) &&
			    (nif->nf_unit == ifp->if_unit) &&
			    !strcmp(nif->nf_name, ifp->if_name)) {
				break;
			}
		if (ifp) {
			qp = &nif->nf_next;
			continue;
		}
		printf("IP Filter: detaching [%s]\n", nif->nf_name);
		*qp = nif->nf_next;

		/*
		 * Disable any rules directly associated with this interface
		 */
		MUTEX_ENTER(&ipf_mutex);
		for (f = ipfilter[0][0]; f; f = f->fr_next)
			if (f->fr_ifa == (void *)nif->nf_ifp)
				f->fr_ifa = (struct ifnet *)-1;
		for (f = ipfilter[1][0]; f; f = f->fr_next)
			if (f->fr_ifa == (void *)nif->nf_ifp)
				f->fr_ifa = (struct ifnet *)-1;
		MUTEX_EXIT(&ipf_mutex);
		MUTEX_ENTER(&ipf_nat);
		for (np = nat_list; np; np = np->in_next)
			if (np->in_ifp == (void *)nif->nf_ifp)
				np->in_ifp =(struct ifnet *)-1;
		MUTEX_EXIT(&ipf_nat);

		KFREE(nif);
		nif = *qp;
	}
	MUTEX_EXIT(&ipfi_mutex);

	nifattach();

	return 0;
}


/*
 * unhook the IP filter from all defined interfaces with IP addresses
 */
static void
nifdetach()
{
	struct ifnet *ifp;
	nif_t *nif, **qp;

	MUTEX_ENTER(&ipfi_mutex); /* sets interrupt priority level to splhi */
	/*
	 * Make two passes, first get rid of all the unknown devices, next
	 * unlink known devices.
	 */
	for (qp = &nif_head; (nif = *qp); ) {
		for (ifp = ifnet; ifp; ifp = ifp->if_next)
			if (nif->nf_ifp == ifp)
				break;
		if (ifp) {
			qp = &nif->nf_next;
			continue;
		}
		printf("IP Filter: removing [%s]\n", nif->nf_name);
		*qp = nif->nf_next;
		KFREE(nif);
	}

	while ((nif = nif_head)) {
		nif_head = nif->nf_next;
		for (ifp = ifnet; ifp; ifp = ifp->if_next)
			if (nif->nf_ifp == ifp)
				break;
		if (ifp) {
			printf("IP Filter: detaching [%s,%d]\n",
				nif->nf_name, ifp->if_unit);

#if	IPFDEBUG >= 4
			printf("IP Filter: nifdetach: ifp(%lx)->if_output FROM %lx TO %lx\n",
				ifp, ifp->if_output, nif->nf_output);
#endif
			ifp->if_output = nif->nf_output;
		}
		KFREE(nif);
	}
	MUTEX_EXIT(&ipfi_mutex);

	return;
}


static void
ipfilterdetach(void)
{
#ifdef IPFILTER_LKM
	MUTEX_ENTER(&ipfi_mutex); /* sets interrupt priority level to splhi */

	if (ipff_addr) {
		*ipff_addr = 0;

		if (ipfk_addr)
			bcopy(ipfk_code, ipfk_addr, sizeof(ipfk_code));

		*ipff_addr = ipff_value;
	}

	MUTEX_EXIT(&ipfi_mutex);
#else
	extern int ipfilterflag;

	ipfilterflag = 0;
#endif
}

/* called by ipldetach() */
void
ipfilter_sgi_detach(void)
{
	nifdetach();

	ipfilterdetach();
}

/* called by iplattach() */
int
ipfilter_sgi_attach(void)
{
	int error;

	nif_interfaces = 0;

	error = ipfilterattach();

	if (!error)
		nifattach();

	return error;
}

/* this function is called from ipfr_slowtimer at 500ms intervals to
   keep our interface list in sync */
void
ipfilter_sgi_intfsync(void)
{
	MUTEX_ENTER(&ipfi_mutex);
	if (nif_interfaces != in_interfaces) {
		/* if the number of interfaces has changed, resync */
		MUTEX_EXIT(&ipfi_mutex);
		ipfsync();
	} else
		MUTEX_EXIT(&ipfi_mutex);
}

#ifdef IPFILTER_LKM
/* this routine should be treated as an interrupt routine and should
   not call any routines that would cause it to sleep, such as: biowait(),
   sleep(), psema() or delay().
*/
int
IPL_EXTERN(unload)(void)
{
	int error = 0;

	error = ipldetach();

	LOCK_DEALLOC(ipl_mutex.l);
	LOCK_DEALLOC(ipf_rw.l);
	LOCK_DEALLOC(ipf_auth.l);
	LOCK_DEALLOC(ipf_natfrag.l);
	LOCK_DEALLOC(ipf_nat.l);
	LOCK_DEALLOC(ipf_state.l);
	LOCK_DEALLOC(ipf_frag.l);
	LOCK_DEALLOC(ipf_mutex.l);
	LOCK_DEALLOC(ipfi_mutex.l);

	return error;
}
#endif

void
IPL_EXTERN(init)(void)
{
#ifdef IPFILTER_LKM
	int error;
#endif

	ipfi_mutex.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_mutex.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_frag.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_state.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_nat.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_natfrag.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_auth.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_rw.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipl_mutex.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);

	if (!ipfi_mutex.l || !ipf_mutex.l || !ipf_frag.l || !ipf_state.l ||
	    !ipf_nat.l || !ipf_natfrag.l || !ipf_auth.l || !ipf_rw.l ||
	    !ipl_mutex.l)
		panic("IP Filter: LOCK_ALLOC failed");

#ifdef IPFILTER_LKM
	error = iplattach();
	if (error) {
		IPL_EXTERN(unload)();
	}
#endif

	return;
}

