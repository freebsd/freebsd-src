/* $FreeBSD$ */

/*
 * Copyright (C) 2012 by Darren Reed.
 * (C)opyright 1997 by Marc Boucher.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
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

#ifndef	MBUF_IS_CLUSTER
# define	MBUF_IS_CLUSTER(m)	((m)->m_flags & MCL_CLUSTER)
#endif
#undef	IPFDEBUG	/* #define IPFDEBUG 9 */

#ifdef IPFILTER_LKM
u_int	ipldevflag = D_MP;
char	*iplmversion = M_VERSION;
#else
u_int	ipfilterdevflag = D_MP;
char	*ipfiltermversion = M_VERSION;
#endif

ipfmutex_t	ipl_mutex, ipfi_mutex, ipf_rw, ipf_stinsert, ipf_auth_mx;
ipfmutex_t	ipf_nat_new, ipf_natio, ipf_timeoutlock;
ipfrwlock_t	ipf_frag, ipf_state, ipf_nat, ipf_natfrag, ipf_auth;
ipfrwlock_t	ipf_global, ipf_mutex, ipf_ipidfrag, ipf_frcache, ipf_tokens;

int     (*ipf_checkp) __P((struct ip *, int, void *, int, mb_t **));

#ifdef IPFILTER_LKM
static int *ipff_addr;
static int ipff_value;
static __psunsigned_t *ipfk_addr;
static __psunsigned_t ipfk_code[4];
#endif
static void nifattach();
static void nifdetach();

typedef	struct	nif	{
	struct	nif	*nf_next;
	struct ifnet	*nf_ifp;
#if (IRIX < 60500)
	int     (*nf_output)(struct ifnet *, struct mbuf *, struct sockaddr *);
#else
	int     (*nf_output)(struct ifnet *, struct mbuf *, struct sockaddr *,
			     struct rtentry *);
#endif
	char	nf_name[LIFNAMSIZ];
	int	nf_unit;
} nif_t;

static nif_t *nif_head;
static int nif_interfaces = 0;
extern int in_interfaces;
#if IRIX >= 60500
toid_t	ipf_timer_id;
#endif

extern ipnat_t *nat_list;

#ifdef IPFDEBUG
static void ipf_dumppacket(m)
	struct mbuf *m;
{
	u_char *s;
	char *t, line[80];
	int len, off, i;

	off = 0;

	while (m != NULL) {
		len = M_LEN(m);
		s = mtod(m, u_char *);
		printf("mbuf 0x%lx len %d flags %x type %d\n",
			m, len, m->m_flags, m->m_type);
		printf("dat 0x%lx off 0x%lx/%d s 0x%lx next 0x%lx\n",
			m->m_dat, m->m_off, m->m_off, s, m->m_next);
		while (len > 0) {
			t = line;
			for (i = 0; (i < 16) && (len > 0); len--, i++)
				sprintf(t, " %02x", *s++), t += strlen(t);
			*s = '\0';
			printf("mbuf:%x:%s\n", off, line);
			off += 16;
		}
		m = m->m_next;
	}
}
#endif


static int
#if IRIX < 60500
ipl_if_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst)
#else
ipl_if_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	      struct rtentry *rt)
#endif
{
#if (IPFDEBUG >= 0)
	static unsigned int cnt = 0;
#endif
	nif_t *nif;

	MUTEX_ENTER(&ipfi_mutex); /* sets interrupt priority level to splhi */
	for (nif = nif_head; nif; nif = nif->nf_next)
		if (nif->nf_ifp == ifp)
			break;
	MUTEX_EXIT(&ipfi_mutex);

	if (nif == NULL) {
		printf("IP Filter: ipl_if_output intf %x NOT FOUND\n", ifp);
		return ENETDOWN;
	}

#if (IPFDEBUG >= 7)
	if ((++cnt % 200) == 0)
		printf("IP Filter: ipl_if_output(ifp=0x%lx, m=0x%lx, dst=0x%lx), m_type=%d m_flags=0x%lx m_off=0x%lx\n", ifp, m, dst, m->m_type, (u_long)m->m_flags, m->m_off);
#endif

	if (ipf_checkp) {
		struct mbuf *m1 = m;
		struct ip *ip;
		int hlen;

		switch(m->m_type)
		{
		case MT_HEADER:
			if (m->m_len == 0) {
				if (m->m_next == NULL)
					break;
				m = m->m_next;
			}
			/* FALLTHROUGH */
		case MT_DATA:
			if (!MBUF_IS_CLUSTER(m) &&
			    ((m->m_off < MMINOFF) || (m->m_off > MMAXOFF))) {
#if (IPFDEBUG >= 4)
				printf("IP Filter: ipl_if_output: bad m_off m_type=%d m_flags=0x%lx m_off=0x%lx\n", m->m_type, (u_long)m->m_flags, m->m_off);
#endif
				break;
			}
			if (m->m_len < sizeof(char)) {
#if (IPFDEBUG >= 3)
				printf("IP Filter: ipl_if_output: mbuf block too small (m_len=%d) for IP vers+hlen, m_type=%d m_flags=0x%lx\n", m->m_len, m->m_type, (u_long)m->m_flags);
#endif
				break;
			}
			ip = mtod(m, struct ip *);
			if (ip->ip_v != IPVERSION) {
#if (IPFDEBUG >= 2)
				ipf_dumppacket(m);
				printf("IP Filter: ipl_if_output: bad ip_v m_type=%d m_flags=0x%lx m_off=0x%lx\n", m->m_type, (u_long)m->m_flags, m->m_off);
#endif
				break;
			}

			hlen = ip->ip_hl << 2;
			if ((*ipf_checkp)(ip, hlen, ifp, 1, &m1) || (m1 == NULL))
				return EHOSTUNREACH;

			m = m1;
			break;

		default:
#if (IPFDEBUG >= 2)
			printf("IP Filter: ipl_if_output: bad m_type=%d m_flags=0x%lxm_off=0x%lx\n", m->m_type, (u_long)m->m_flags, m->m_off);
#endif
			break;
		}
	}
#if (IRIX < 60500)
	return (*nif->nf_output)(ifp, m, dst);
#else
	return (*nif->nf_output)(ifp, m, dst, rt);
#endif
}

int


#if !defined(IPFILTER_LKM) && (IRIX >= 60500)
ipfilter_kernel(struct ifnet *rcvif, struct mbuf *m)
#else
ipl_kernel(struct ifnet *rcvif, struct mbuf *m)
#endif
{
#if (IPFDEBUG >= 7)
	static unsigned int cnt = 0;

	if ((++cnt % 200) == 0)
		printf("IP Filter: ipl_kernel(rcvif=0x%lx, m=0x%lx\n",
			rcvif, m);
#endif

	if (ipf_running <= 0)
		return IPF_ACCEPTIT;

	/*
	 * Check if we want to allow this packet to be processed.
	 * Consider it to be bad if not.
	 */
	if (ipf_checkp) {
		struct mbuf *m1 = m;
		struct ip *ip;
		int hlen;

		if ((m->m_type != MT_DATA) && (m->m_type != MT_HEADER)) {
#if (IPFDEBUG >= 4)
			printf("IP Filter: ipl_kernel: bad m_type=%d m_flags=0x%lx m_off=0x%lx\n", m->m_type, (u_long)m->m_flags, m->m_off);
#endif
			return IPF_ACCEPTIT;
		}

		if (!MBUF_IS_CLUSTER(m) &&
		    ((m->m_off < MMINOFF) || (m->m_off > MMAXOFF))) {
#if (IPFDEBUG >= 4)
			printf("IP Filter: ipl_kernel: bad m_off m_type=%d m_flags=0x%lx m_off=0x%lx\n", m->m_type, (u_long)m->m_flags, m->m_off);
#endif
			return IPF_ACCEPTIT;
		}

		if (m->m_len < sizeof(char)) {
#if (IPFDEBUG >= 1)
			printf("IP Filter: ipl_kernel: mbuf block too small (m_len=%d) for IP vers+hlen, m_type=%d m_flags=0x%lx\n", m->m_len, m->m_type, (u_long)m->m_flags);
#endif
			return IPF_ACCEPTIT;
		}

		ip = mtod(m, struct ip *);
		if (ip->ip_v != IPVERSION) {
#if (IPFDEBUG >= 4)
			printf("IP Filter: ipl_kernel: bad ip_v\n");
#endif
			m_freem(m);
			return IPF_DROPIT;
		}

		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
		hlen = ip->ip_hl << 2;
		if ((*ipf_checkp)(ip, hlen, rcvif, 0, &m1) || !m1)
			return IPF_DROPIT;
		ip = mtod(m1, struct ip *);
		ip->ip_len = ntohs(ip->ip_len);
		ip->ip_off = ntohs(ip->ip_off);

#if (IPFDEBUG >= 2)
		if (m != m1)
			printf("IP Filter: ipl_kernel: m != m1\n");
#endif
	}

	return IPF_ACCEPTIT;
}

int
ipl_ipfilter_attach(void)
{
#if defined(IPFILTER_LKM)
	__psunsigned_t *addr_ff, *addr_fk;

	st_findaddr("ipfilterflag", &addr_ff);
# if (IPFDEBUG >= 1)
	printf("IP Filter: st_findaddr ipfilterflag=0x%lx\n", addr_ff);
# endif
	if (!addr_ff)
		return ESRCH;

	st_findaddr("ipfilter_kernel", &addr_fk);
# if (IPFDEBUG >= 1)
	printf("IP Filter: st_findaddr ipfilter_kernel=0x%lx\n", addr_fk);
# endif
	if (!addr_fk)
		return ESRCH;

	MUTEX_ENTER(&ipfi_mutex); /* sets interrupt priority level to splhi */

	ipff_addr = (int *)addr_ff;

	ipff_value = *ipff_addr;
	*ipff_addr = 0;


	ipfk_addr = addr_fk;

	bcopy(ipfk_addr, ipfk_code, sizeof(ipfk_code));

	/* write a "li t4, ipl_kernel" instruction */
	ipfk_addr[0] = 0x3c0c0000 |
		       (((__psunsigned_t)ipl_kernel >> 16) & 0xffff);
	ipfk_addr[1] = 0x358c0000 |
		       ((__psunsigned_t)ipl_kernel & 0xffff);
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
	nif_interfaces = 0;
	nifattach();

	return 0;
}


/*
 * attach the packet filter to each non-loopback interface that is running
 */
static void
nifattach()
{
	nif_t *nif, *qf2;
	struct ifnet *ifp;
	struct frentry *f;
	ipnat_t *np;

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
#if (IPFDEBUG >= 2)
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
		(void) strncpy(nif->nf_name, ifp->if_name,
			       sizeof(nif->nf_name));
		nif->nf_name[sizeof(nif->nf_name) - 1] = '\0';
		nif->nf_unit = ifp->if_unit;

		nif->nf_next = nif_head;
		nif_head = nif;

		/*
		 * Activate any rules directly associated with this interface
		 */
		WRITE_ENTER(&ipf_mutex);
		for (f = ipf_rules[0][0]; f; f = f->fr_next) {
			if ((f->fr_ifa == (struct ifnet *)-1)) {
				if (f->fr_ifname[0] &&
				    (GETIFP(f->fr_ifname, 4) == ifp))
					f->fr_ifa = ifp;
			}
		}
		for (f = ipf_rules[1][0]; f; f = f->fr_next) {
			if ((f->fr_ifa == (struct ifnet *)-1)) {
				if (f->fr_ifname[0] &&
				    (GETIFP(f->fr_ifname, 4) == ifp))
					f->fr_ifa = ifp;
			}
		}
		RWLOCK_EXIT(&ipf_mutex);
		WRITE_ENTER(&ipf_nat);
		for (np = nat_list; np; np = np->in_next) {
			if ((np->in_ifps[0] == (void *)-1)) {
				if (np->in_ifnames[0][0] &&
				    (GETIFP(np->in_ifnames[0], 4) == ifp))
					np->in_ifps[0] = (void *)ifp;
			}
			if ((np->in_ifps[1] == (void *)-1)) {
				if (np->in_ifnames[1][0] &&
				    (GETIFP(np->in_ifnames[1], 4) == ifp))
					np->in_ifps[1] = (void *)ifp;
			}
		}
		RWLOCK_EXIT(&ipf_nat);

		nif->nf_output = ifp->if_output;
		ifp->if_output = ipl_if_output;

#if (IPFDEBUG >= 2)
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
 * unhook the IP filter from all defined interfaces with IP addresses
 */
static void
nifdetach()
{
	nif_t *nif, *qf2, **qp;
	struct ifnet *ifp;

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

#if (IPFDEBUG >= 4)
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


void
ipl_ipfilter_detach(void)
{
#ifdef IPFILTER_LKM
	nifdetach();
	MUTEX_ENTER(&ipfi_mutex); /* sets interrupt priority level to splhi */

	if (ipff_addr) {
		*ipff_addr = 0;

		if (ipfk_addr) {
			bcopy(ipfk_code, ipfk_addr, sizeof(ipfk_code));
			icache_inval(ipfk_addr - 16, sizeof(ipfk_code)+32);
		}

		*ipff_addr = ipff_value;
	}

	MUTEX_EXIT(&ipfi_mutex);
#else
	extern int ipfilterflag;

	nifdetach();

	ipfilterflag = 0;
#endif
}


/* this function is called from ipf_slowtimer at 500ms intervals to
   keep our interface list in sync */
void
ipl_ipfilter_intfsync(void)
{
	MUTEX_ENTER(&ipfi_mutex);
	if (nif_interfaces != in_interfaces) {
		/* if the number of interfaces has changed, resync */
		MUTEX_EXIT(&ipfi_mutex);
		ipf_sync(&ipfmain, NULL);
	} else
		MUTEX_EXIT(&ipfi_mutex);
}

#ifdef IPFILTER_LKM
/* this routine should be treated as an interrupt routine and should
   not call any routines that would cause it to sleep, such as: biowait(),
   sleep(), psema() or delay().
*/
int
iplunload(void)
{
	int error = 0;

	if (ipf_refcnt)
		return EBUSY;

	WRITE_ENTER(&ipf_global);
	error = ipl_detach();
	if (error != 0) {
		RWLOCK_EXIT(&ipf_global);
		return error;
	}
	ipf_running = -2;

#if (IRIX < 60500)
	LOCK_DEALLOC(ipl_mutex.l);
	LOCK_DEALLOC(ipf_rw.l);
	LOCK_DEALLOC(ipf_auth.l);
	LOCK_DEALLOC(ipf_natfrag.l);
	LOCK_DEALLOC(ipf_ipidfrag.l);
	LOCK_DEALLOC(ipf_tokens.l);
	LOCK_DEALLOC(ipf_stinsert.l);
	LOCK_DEALLOC(ipf_nat_new.l);
	LOCK_DEALLOC(ipf_natio.l);
	LOCK_DEALLOC(ipf_nat.l);
	LOCK_DEALLOC(ipf_state.l);
	LOCK_DEALLOC(ipf_frag.l);
	LOCK_DEALLOC(ipf_auth_mx.l);
	LOCK_DEALLOC(ipf_mutex.l);
	LOCK_DEALLOC(ipf_frcache.l);
	LOCK_DEALLOC(ipfi_mutex.l);
	RWLOCK_EXIT(&ipf_global);
	LOCK_DEALLOC(ipf_global.l);
#else
	MUTEX_DESTROY(&ipf_rw);
	MUTEX_DESTROY(&ipfi_mutex);
	MUTEX_DESTROY(&ipf_timeoutlock);
	RW_DESTROY(&ipf_mutex);
	RW_DESTROY(&ipf_frcache);
	RW_DESTROY(&ipf_tokens);
	RWLOCK_EXIT(&ipf_global);
	delay(hz);
	RW_DESTROY(&ipf_global);
#endif

	printf("%s unloaded\n", ipfilter_version);

	delay(hz);

	return 0;
}
#endif

void
ipfilterinit(void)
{
#ifdef IPFILTER_LKM
	int error;
#endif

#if (IRIX < 60500)
	ipfi_mutex.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
ipf_mutex.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
ipf_frcache.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
ipf_timeoutlock.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_global.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_frag.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_state.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_nat.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_stinsert.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_natfrag.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_ipidfrag.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_tokens.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_auth.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipf_rw.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);
	ipl_mutex.l = LOCK_ALLOC((uchar_t)-1, IPF_LOCK_PL, (lkinfo_t *)-1, KM_NOSLEEP);

	if (!ipfi_mutex.l || !ipf_mutex.l || !ipf_timeoutlock.l ||
	    !ipf_frag.l || !ipf_state.l || !ipf_nat.l || !ipf_natfrag.l ||
	    !ipf_auth.l || !ipf_rw.l || !ipf_ipidfrag.l || !ipl_mutex.l ||
	    !ipf_stinsert.l || !ipf_auth_mx.l || !ipf_frcache.l ||
	    !ipf_tokens.l)
		panic("IP Filter: LOCK_ALLOC failed");
#else
	MUTEX_INIT(&ipf_rw, "ipf rw mutex");
	MUTEX_INIT(&ipf_timeoutlock, "ipf timeout mutex");
	RWLOCK_INIT(&ipf_global, "ipf filter load/unload mutex");
	RWLOCK_INIT(&ipf_mutex, "ipf filter rwlock");
	RWLOCK_INIT(&ipf_frcache, "ipf cache rwlock");
#endif

#ifdef IPFILTER_LKM
	error = ipl_attach();
	if (error) {
		iplunload();
	} else {
		char *defpass;

		if (FR_ISPASS(ipf_pass))
			defpass = "pass";
		else if (FR_ISBLOCK(ipf_pass))
			defpass = "block";
		else
			defpass = "no-match -> block";

		printf("%s initialized.  Default = %s all, Logging = %s%s\n",
			ipfilter_version, defpass,
# ifdef  IPFILTER_LOG
			"enabled",
# else
			"disabled",
# endif
# ifdef IPFILTER_COMPILED
		" (COMPILED)"
# else
		""
# endif
		);
	}
#endif

	return;
}
