/*
 * Copyright (C) 1993-2000 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * I hate legaleese, don't you ?
 */
#if !defined(lint)
static const char sccsid[] = "%W% %G% (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id: ip_sfil.c,v 2.23.2.8 2000/10/19 15:42:10 darrenr Exp $";
#endif

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/cpuvar.h>
#include <sys/open.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ksynch.h>
#include <sys/kmem.h>
#include <sys/mkdev.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/dditypes.h>
#include <sys/cmn_err.h>
#include <net/if.h>
#include <net/af.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include "ip_compat.h"
#ifdef	USE_INET6
# include <netinet/icmp6.h>
#endif
#include "ip_fil.h"
#include "ip_state.h"
#include "ip_nat.h"
#include "ip_frag.h"
#include "ip_auth.h"
#include "ip_proxy.h"
#include <inet/ip_ire.h>
#ifndef	MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif


extern	fr_flags, fr_active;

int	fr_running = 0;
int	ipl_unreach = ICMP_UNREACH_HOST;
u_long	ipl_frouteok[2] = {0, 0};
static	int	frzerostats __P((caddr_t));

static	int	frrequest __P((minor_t, int, caddr_t, int));
static	int	send_ip __P((fr_info_t *fin, mblk_t *m));
kmutex_t	ipl_mutex, ipf_authmx, ipf_rw, ipf_hostmap;
KRWLOCK_T	ipf_mutex, ipfs_mutex, ipf_solaris;
KRWLOCK_T	ipf_frag, ipf_state, ipf_nat, ipf_natfrag, ipf_auth;
kcondvar_t	iplwait, ipfauthwait;


int ipldetach()
{
	int	i;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "ipldetach()\n");
#endif
#ifdef	IPFILTER_LOG
	for (i = IPL_LOGMAX; i >= 0; i--)
		ipflog_clear(i);
#endif
	i = FR_INQUE|FR_OUTQUE;
	(void) frflush(IPL_LOGIPF, FR_INQUE|FR_OUTQUE);
	ipfr_unload();
	fr_stateunload();
	ip_natunload();
	cv_destroy(&iplwait);
	cv_destroy(&ipfauthwait);
	mutex_destroy(&ipf_hostmap);
	mutex_destroy(&ipf_authmx);
	mutex_destroy(&ipl_mutex);
	mutex_destroy(&ipf_rw);
	RW_DESTROY(&ipf_mutex);
	RW_DESTROY(&ipf_frag);
	RW_DESTROY(&ipf_state);
	RW_DESTROY(&ipf_natfrag);
	RW_DESTROY(&ipf_nat);
	RW_DESTROY(&ipf_auth);
	RW_DESTROY(&ipfs_mutex);
	/* NOTE: This lock is acquired in ipf_detach */
	RWLOCK_EXIT(&ipf_solaris);
	RW_DESTROY(&ipf_solaris);
	return 0;
}


int iplattach __P((void))
{
#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplattach()\n");
#endif
	bzero((char *)frcache, sizeof(frcache));
	mutex_init(&ipf_rw, "ipf rw mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipl_mutex, "ipf log mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_authmx, "ipf auth log mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_hostmap, "ipf hostmap mutex", MUTEX_DRIVER, NULL);
	RWLOCK_INIT(&ipf_solaris, "ipf filter load/unload mutex", NULL);
	RWLOCK_INIT(&ipf_mutex, "ipf filter rwlock", NULL);
	RWLOCK_INIT(&ipfs_mutex, "ipf solaris mutex", NULL);
	RWLOCK_INIT(&ipf_frag, "ipf fragment rwlock", NULL);
	RWLOCK_INIT(&ipf_state, "ipf IP state rwlock", NULL);
	RWLOCK_INIT(&ipf_nat, "ipf IP NAT rwlock", NULL);
	RWLOCK_INIT(&ipf_natfrag, "ipf IP NAT-Frag rwlock", NULL);
	RWLOCK_INIT(&ipf_auth, "ipf IP User-Auth rwlock", NULL);
	cv_init(&iplwait, "ipl condvar", CV_DRIVER, NULL);
	cv_init(&ipfauthwait, "ipf auth condvar", CV_DRIVER, NULL);
#ifdef	IPFILTER_LOG
	ipflog_init();
#endif
	if (nat_init() == -1)
		return -1;
	if (fr_stateinit() == -1)
		return -1;
	if (appr_init() == -1)
		return -1;
	return 0;
}


static	int	frzerostats(data)
caddr_t	data;
{
	friostat_t fio;
	int error;

	fr_getstat(&fio);
	error = IWCOPYPTR((caddr_t)&fio, data, sizeof(fio));
	if (error)
		return error;

	bzero((char *)frstats, sizeof(*frstats) * 2);

	return 0;
}


/*
 * Filter ioctl interface.
 */
int iplioctl(dev, cmd, data, mode, cp, rp)
dev_t dev;
int cmd;
#if SOLARIS2 >= 7
intptr_t data;
#else
int *data;
#endif
int mode;
cred_t *cp;
int *rp;
{
	int error = 0, tmp;
	minor_t unit;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplioctl(%x,%x,%x,%d,%x,%d)\n",
		dev, cmd, data, mode, cp, rp);
#endif
	unit = getminor(dev);
	if (IPL_LOGMAX < unit)
		return ENXIO;

	if (fr_running <= 0)
		return 0;

	READ_ENTER(&ipf_solaris);
	if (unit == IPL_LOGNAT) {
		error = nat_ioctl((caddr_t)data, cmd, mode);
		RWLOCK_EXIT(&ipf_solaris);
		return error;
	}
	if (unit == IPL_LOGSTATE) {
		error = fr_state_ioctl((caddr_t)data, cmd, mode);
		RWLOCK_EXIT(&ipf_solaris);
		return error;
	}
	if (unit == IPL_LOGAUTH) {
		error = fr_auth_ioctl((caddr_t)data, cmd, NULL, NULL);
		RWLOCK_EXIT(&ipf_solaris);
		return error;
	}

	switch (cmd) {
	case SIOCFRENB :
	{
		u_int	enable;

		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = IRCOPY((caddr_t)data, (caddr_t)&enable,
				       sizeof(enable));
		break;
	}
	case SIOCSETFF :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			WRITE_ENTER(&ipf_mutex);
			error = IRCOPY((caddr_t)data, (caddr_t)&fr_flags,
			       sizeof(fr_flags));
			RWLOCK_EXIT(&ipf_mutex);
		}
		break;
	case SIOCGETFF :
		error = IWCOPY((caddr_t)&fr_flags, (caddr_t)data,
			       sizeof(fr_flags));
		if (error)
			error = EFAULT;
		break;
	case SIOCINAFR :
	case SIOCRMAFR :
	case SIOCADAFR :
	case SIOCZRLST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, (caddr_t)data, fr_active);
		break;
	case SIOCINIFR :
	case SIOCRMIFR :
	case SIOCADIFR :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, (caddr_t)data,
					  1 - fr_active);
		break;
	case SIOCSWAPA :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			WRITE_ENTER(&ipf_mutex);
			bzero((char *)frcache, sizeof(frcache[0]) * 2);
			error = IWCOPY((caddr_t)&fr_active, (caddr_t)data,
				       sizeof(fr_active));
			if (error)
				error = EFAULT;
			fr_active = 1 - fr_active;
			RWLOCK_EXIT(&ipf_mutex);
		}
		break;
	case SIOCGETFS :
	{
		friostat_t	fio;

		READ_ENTER(&ipf_mutex);
		fr_getstat(&fio);
		RWLOCK_EXIT(&ipf_mutex);
		error = IWCOPYPTR((caddr_t)&fio, (caddr_t)data, sizeof(fio));
		if (error)
			error = EFAULT;
		break;
	}
	case SIOCFRZST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frzerostats((caddr_t)data);
		break;
	case	SIOCIPFFL :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = IRCOPY((caddr_t)data, (caddr_t)&tmp,
				       sizeof(tmp));
			if (!error) {
				tmp = frflush(unit, tmp);
				error = IWCOPY((caddr_t)&tmp, (caddr_t)data,
					       sizeof(tmp));
				if (error)
					error = EFAULT;
			}
		}
		break;
	case SIOCSTLCK :
		error = IRCOPY((caddr_t)data, (caddr_t)&tmp, sizeof(tmp));
		if (!error) {
			fr_state_lock = tmp;
			fr_nat_lock = tmp;
			fr_frag_lock = tmp;
			fr_auth_lock = tmp;
		} else
			error = EFAULT;
	break;
#ifdef	IPFILTER_LOG
	case	SIOCIPFFB :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			tmp = ipflog_clear(unit);
			error = IWCOPY((caddr_t)&tmp, (caddr_t)data,
				       sizeof(tmp));
			if (error)
				error = EFAULT;
		}
		break;
#endif /* IPFILTER_LOG */
	case SIOCFRSYN :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = ipfsync();
		break;
	case SIOCGFRST :
		error = IWCOPYPTR((caddr_t)ipfr_fragstats(), (caddr_t)data,
				  sizeof(ipfrstat_t));
		break;
	case FIONREAD :
	{
#ifdef	IPFILTER_LOG
		int copy = (int)iplused[IPL_LOGIPF];

		error = IWCOPY((caddr_t)&copy, (caddr_t)data, sizeof(copy));
		if (error)
			error = EFAULT;
#endif
		break;
	}
	default :
		error = EINVAL;
		break;
	}
	RWLOCK_EXIT(&ipf_solaris);
	return error;
}


ill_t	*get_unit(name, v)
char	*name;
int	v;
{
	size_t len = strlen(name) + 1;	/* includes \0 */
	ill_t *il;
	int sap;

	if (v == 4)
		sap = 0x0800;
	else if (v == 6)
		sap = 0x86dd;
	else
		return NULL;
	for (il = ill_g_head; il; il = il->ill_next)
		if ((len == il->ill_name_length) && (il->ill_sap == sap) &&
		    !strncmp(il->ill_name, name, len))
			return il;
	return NULL;
}


static int frrequest(unit, req, data, set)
minor_t unit;
int req, set;
caddr_t data;
{
	register frentry_t *fp, *f, **fprev;
	register frentry_t **ftail;
	frentry_t fr;
	frdest_t *fdp;
	frgroup_t *fg = NULL;
	u_int   *p, *pp;
	int error = 0, in;
	u_32_t group;
	ill_t *ill;
	ipif_t *ipif;
	ire_t *ire;

	fp = &fr;
	error = IRCOPYPTR(data, (caddr_t)fp, sizeof(*fp));
	if (error)
		return EFAULT;
	fp->fr_ref = 0;
#if SOLARIS2 >= 8
	if (fp->fr_v == 4)
		fp->fr_sap = IP_DL_SAP;
	else if (fp->fr_v == 6)
		fp->fr_sap = IP6_DL_SAP;
	else
		return EINVAL;
#else
	fp->fr_sap = 0;
#endif

	WRITE_ENTER(&ipf_mutex);
	/*
	 * Check that the group number does exist and that if a head group
	 * has been specified, doesn't exist.
	 */
	if ((req != SIOCZRLST) && fp->fr_grhead &&
	    fr_findgroup(fp->fr_grhead, fp->fr_flags, unit, set, NULL)) {
		error = EEXIST;
		goto out;
	}
	if ((req != SIOCZRLST) && fp->fr_group &&
	    !fr_findgroup(fp->fr_group, fp->fr_flags, unit, set, NULL)) {
		error = ESRCH;
		goto out;
	}

	in = (fp->fr_flags & FR_INQUE) ? 0 : 1;

	if (unit == IPL_LOGAUTH)
		ftail = fprev = &ipauth;
	else if ((fp->fr_flags & FR_ACCOUNT) && (fp->fr_v == 4))
		ftail = fprev = &ipacct[in][set];
	else if ((fp->fr_flags & (FR_OUTQUE|FR_INQUE)) && (fp->fr_v == 4))
		ftail = fprev = &ipfilter[in][set];
#ifdef	USE_INET6
	else if ((fp->fr_flags & FR_ACCOUNT) && (fp->fr_v == 6))
		ftail = fprev = &ipacct6[in][set];
	else if ((fp->fr_flags & (FR_OUTQUE|FR_INQUE)) && (fp->fr_v == 6))
		ftail = fprev = &ipfilter6[in][set];
#endif
	else {
		error = ESRCH;
		goto out;
	}

	group = fp->fr_group;
	if (group != 0) {
		fg = fr_findgroup(group, fp->fr_flags, unit, set, NULL);
		if (fg == NULL) {
			error = ESRCH;
			goto out;
		}
		ftail = fprev = fg->fg_start;
	}

	bzero((char *)frcache, sizeof(frcache[0]) * 2);

	if (*fp->fr_ifname) {
		fp->fr_ifa = (void *)get_unit((char *)fp->fr_ifname,
					      (int)fp->fr_v);
		if (!fp->fr_ifa)
			fp->fr_ifa = (struct ifnet *)-1;
	}

	fdp = &fp->fr_dif;
	fp->fr_flags &= ~FR_DUP;
	if (*fdp->fd_ifname) {
		ill = get_unit(fdp->fd_ifname, (int)fp->fr_v);
		if (!ill)
			ire = (ire_t *)-1;
		else if ((ipif = ill->ill_ipif) && (fp->fr_v == 4)) {
#if SOLARIS2 > 5
			ire = ire_ctable_lookup(ipif->ipif_local_addr, 0,
						IRE_LOCAL, NULL, NULL,
						MATCH_IRE_TYPE);
#else
			ire = ire_lookup_myaddr(ipif->ipif_local_addr);
#endif
			if (!ire)
				ire = (ire_t *)-1;
			else
				fp->fr_flags |= FR_DUP;
		}
#ifdef	USE_INET6
		else if ((ipif = ill->ill_ipif) && (fp->fr_v == 6)) {
			ire = ire_ctable_lookup_v6(&ipif->ipif_v6lcl_addr, 0,
						   IRE_LOCAL, NULL, NULL,
						   MATCH_IRE_TYPE);
			if (!ire)
				ire = (ire_t *)-1;
			else
				fp->fr_flags |= FR_DUP;
		}
#endif
		fdp->fd_ifp = (struct ifnet *)ire;
	}

	fdp = &fp->fr_tif;
	if (*fdp->fd_ifname) {
		ill = get_unit(fdp->fd_ifname, (int)fp->fr_v);
		if (!ill)
			ire = (ire_t *)-1;
		else if ((ipif = ill->ill_ipif) && (fp->fr_v == 4)) {
#if SOLARIS2 > 5
			ire = ire_ctable_lookup(ipif->ipif_local_addr, 0,
						IRE_LOCAL, NULL, NULL,
						MATCH_IRE_TYPE);
#else
			ire = ire_lookup_myaddr(ipif->ipif_local_addr);
#endif
			if (!ire)
				ire = (ire_t *)-1;
		}
#ifdef	USE_INET6
		else if ((ipif = ill->ill_ipif) && (fp->fr_v == 6)) {
			ire = ire_ctable_lookup_v6(&ipif->ipif_v6lcl_addr, 0,
						   IRE_LOCAL, NULL, NULL,
						   MATCH_IRE_TYPE);
			if (!ire)
				ire = (ire_t *)-1;
		}
#endif
		fdp->fd_ifp = (struct ifnet *)ire;
	}

	/*
	 * Look for a matching filter rule, but don't include the next or
	 * interface pointer in the comparison (fr_next, fr_ifa).
	 */
	for (fp->fr_cksum = 0, p = (u_int *)&fp->fr_ip, pp = &fp->fr_cksum;
	     p < pp; p++)
		fp->fr_cksum += *p;

	for (; (f = *ftail); ftail = &f->fr_next)
		if ((fp->fr_cksum == f->fr_cksum) &&
		    !bcmp((char *)&f->fr_ip, (char *)&fp->fr_ip, FR_CMPSIZ))
			break;

	/*
	 * If zero'ing statistics, copy current to caller and zero.
	 */
	if (req == SIOCZRLST) {
		if (!f) {
			error = ESRCH;
			goto out;
		}
		MUTEX_DOWNGRADE(&ipf_mutex);
		error = IWCOPYPTR((caddr_t)f, data, sizeof(*f));
		if (error)
			goto out;
		f->fr_hits = 0;
		f->fr_bytes = 0;
		goto out;
	}

	if (!f) {
		if (req != SIOCINAFR && req != SIOCINIFR)
			while ((f = *ftail))
				ftail = &f->fr_next;
		else {
			if (fp->fr_hits) {
				ftail = fprev;
				while (--fp->fr_hits && (f = *ftail))
					ftail = &f->fr_next;
			}
			f = NULL;
		}
	}

	if (req == SIOCRMAFR || req == SIOCRMIFR) {
		if (!f)
			error = ESRCH;
		else {
			/*
			 * Only return EBUSY if there is a group list, else
			 * it's probably just state information referencing
			 * the rule.
			 */
			if ((f->fr_ref > 1) && f->fr_grp) {
				error = EBUSY;
				goto out;
			}
			if (fg && fg->fg_head)
				fg->fg_head->fr_ref--;
			if (unit == IPL_LOGAUTH) {
				error = fr_auth_ioctl(data, req, fp, ftail);
				goto out;
			}
			if (f->fr_grhead)
				fr_delgroup(f->fr_grhead, fp->fr_flags,
					    unit, set);
			fixskip(fprev, f, -1);
			*ftail = f->fr_next;
			f->fr_next = NULL;
			if (f->fr_ref == 0)
				KFREE(f);
		}
	} else {
		if (f) {
			error = EEXIST;
		} else {
			if (unit == IPL_LOGAUTH) {
				error = fr_auth_ioctl(data, req, fp, ftail);
				goto out;
			}
			KMALLOC(f, frentry_t *);
			if (f != NULL) {
				if (fg && fg->fg_head)
					fg->fg_head->fr_ref++;
				bcopy((char *)fp, (char *)f, sizeof(*f));
				f->fr_ref = 1;
				f->fr_hits = 0;
				f->fr_next = *ftail;
				*ftail = f;
				if (req == SIOCINIFR || req == SIOCINAFR)
					fixskip(fprev, f, 1);
				f->fr_grp = NULL;
				group = f->fr_grhead;
				if (group != 0)
					fg = fr_addgroup(group, f, unit, set);
			} else
				error = ENOMEM;
		}
	}
out:
	RWLOCK_EXIT(&ipf_mutex);
	return (error);
}


/*
 * routines below for saving IP headers to buffer
 */
int iplopen(devp, flags, otype, cred)
dev_t *devp;
int flags, otype;
cred_t *cred;
{
	minor_t min = getminor(*devp);

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplopen(%x,%x,%x,%x)\n", devp, flags, otype, cred);
#endif
	if ((fr_running <= 0) || !(otype & OTYP_CHR))
		return ENXIO;
	min = (IPL_LOGMAX < min) ? ENXIO : 0;
	return min;
}


int iplclose(dev, flags, otype, cred)
dev_t dev;
int flags, otype;
cred_t *cred;
{
	minor_t	min = getminor(dev);

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplclose(%x,%x,%x,%x)\n", dev, flags, otype, cred);
#endif
	min = (IPL_LOGMAX < min) ? ENXIO : 0;
	return min;
}

#ifdef	IPFILTER_LOG
/*
 * iplread/ipllog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
int iplread(dev, uio, cp)
dev_t dev;
register struct uio *uio;
cred_t *cp;
{
#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplread(%x,%x,%x)\n", dev, uio, cp);
#endif
	return ipflog_read(getminor(dev), uio);
}
#endif /* IPFILTER_LOG */


/*
 * send_reset - this could conceivably be a call to tcp_respond(), but that
 * requires a large amount of setting up and isn't any more efficient.
 */
int send_reset(oip, fin)
ip_t *oip;
fr_info_t *fin;
{
	tcphdr_t *tcp, *tcp2;
	int tlen, hlen;
	mblk_t *m;
#ifdef	USE_INET6
	ip6_t *ip6, *oip6 = (ip6_t *)oip;
#endif
	ip_t *ip;

	tcp = (struct tcphdr *)fin->fin_dp;
	if (tcp->th_flags & TH_RST)
		return -1;
	tlen = (tcp->th_flags & (TH_SYN|TH_FIN)) ? 1 : 0;
#ifdef	USE_INET6
	if (fin->fin_v == 6)
		hlen = sizeof(ip6_t);
	else
#endif
		hlen = sizeof(ip_t);
	hlen += sizeof(*tcp2);
	if ((m = (mblk_t *)allocb(hlen + 16, BPRI_HI)) == NULL)
		return -1;

	m->b_rptr += 16;
	MTYPE(m) = M_DATA;
	m->b_wptr = m->b_rptr + hlen;
	bzero((char *)m->b_rptr, hlen);
	tcp2 = (struct tcphdr *)(m->b_rptr + hlen - sizeof(*tcp2));
	tcp2->th_dport = tcp->th_sport;
	tcp2->th_sport = tcp->th_dport;
	if (tcp->th_flags & TH_ACK) {
		tcp2->th_seq = tcp->th_ack;
		tcp2->th_flags = TH_RST|TH_ACK;
	} else {
		tcp2->th_ack = ntohl(tcp->th_seq);
		tcp2->th_ack += tlen;
		tcp2->th_ack = htonl(tcp2->th_ack);
		tcp2->th_flags = TH_RST;
	}
	tcp2->th_off = sizeof(struct tcphdr) >> 2;
	tcp2->th_flags = TH_RST|TH_ACK;

	/*
	 * This is to get around a bug in the Solaris 2.4/2.5 TCP checksum
	 * computation that is done by their put routine.
	 */
	tcp2->th_sum = htons(0x14);
#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		ip6 = (ip6_t *)m->b_rptr;
		ip6->ip6_src = oip6->ip6_dst;
		ip6->ip6_dst = oip6->ip6_src;
		ip6->ip6_plen = htons(sizeof(*tcp));
		ip6->ip6_nxt = IPPROTO_TCP;
	} else
#endif
	{
		ip = (ip_t *)m->b_rptr;
		ip->ip_src.s_addr = oip->ip_dst.s_addr;
		ip->ip_dst.s_addr = oip->ip_src.s_addr;
		ip->ip_hl = sizeof(*ip) >> 2;
		ip->ip_p = IPPROTO_TCP;
		ip->ip_len = htons(sizeof(*ip) + sizeof(*tcp));
		ip->ip_tos = oip->ip_tos;
	}
	return send_ip(fin, m);
}


int static send_ip(fin, m)
fr_info_t *fin;
mblk_t *m;
{
	RWLOCK_EXIT(&ipfs_mutex);
	RWLOCK_EXIT(&ipf_solaris);
#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		extern void ip_wput_v6 __P((queue_t *, mblk_t *));
		ip6_t *ip6;

		ip6 = (ip6_t *)m->b_rptr;
		ip6->ip6_flow = 0;
		ip6->ip6_vfc = 0x60;
		ip6->ip6_hlim = 127;
		ip_wput_v6(((qif_t *)fin->fin_qif)->qf_ill->ill_wq, m);
	} else
#endif
	{
		ip_t *ip;

		ip = (ip_t *)m->b_rptr;
		ip->ip_v = IPVERSION;
		ip->ip_ttl = 60;
		ip_wput(((qif_t *)fin->fin_qif)->qf_ill->ill_wq, m);
	}
	READ_ENTER(&ipf_solaris);
	READ_ENTER(&ipfs_mutex);
	return 0;
}


int send_icmp_err(oip, type, fin, dst)
ip_t *oip;
int type;
fr_info_t *fin;
int dst;
{
	struct in_addr dst4;
	struct icmp *icmp;
	mblk_t *m, *mb;
	int hlen, code;
	qif_t *qif;
	u_short sz;
	ill_t *il;
#ifdef	USE_INET6
	ip6_t *ip6, *oip6;
#endif
	ip_t *ip;

	if ((type < 0) || (type > ICMP_MAXTYPE))
		return -1;

	code = fin->fin_icode;
#ifdef USE_INET6
	if ((code < 0) || (code > sizeof(icmptoicmp6unreach)/sizeof(int)))
		return -1;
#endif

	qif = fin->fin_qif;
	m = fin->fin_qfm;

#ifdef	USE_INET6
	if (oip->ip_v == 6) {
		oip6 = (ip6_t *)oip;
		sz = sizeof(ip6_t);
		sz += MIN(m->b_wptr - m->b_rptr, 512);
		hlen = sizeof(ip6_t);
		type = icmptoicmp6types[type];
		if (type == ICMP6_DST_UNREACH)
			code = icmptoicmp6unreach[code];
	} else
#endif
	{
		if ((oip->ip_p == IPPROTO_ICMP) &&
		    !(fin->fin_fi.fi_fl & FI_SHORT))
			switch (ntohs(fin->fin_data[0]) >> 8)
			{
			case ICMP_ECHO :
			case ICMP_TSTAMP :
			case ICMP_IREQ :
			case ICMP_MASKREQ :
				break;
			default :
				return 0;
			}

		sz = sizeof(ip_t) * 2;
		sz += 8;		/* 64 bits of data */
		hlen = sz;
	}

	sz += offsetof(struct icmp, icmp_ip);
	if ((mb = (mblk_t *)allocb((size_t)sz + 16, BPRI_HI)) == NULL)
		return -1;
	MTYPE(mb) = M_DATA;
	mb->b_rptr += 16;
	mb->b_wptr = mb->b_rptr + sz;
	bzero((char *)mb->b_rptr, (size_t)sz);
	icmp = (struct icmp *)(mb->b_rptr + sizeof(*ip));
	icmp->icmp_type = type;
	icmp->icmp_code = code;
	icmp->icmp_cksum = 0;
#ifdef	icmp_nextmtu
	if (type == ICMP_UNREACH && (il = qif->qf_ill) &&
	    fin->fin_icode == ICMP_UNREACH_NEEDFRAG)
		icmp->icmp_nextmtu = htons(il->ill_max_frag);
#endif

#ifdef	USE_INET6
	if (oip->ip_v == 6) {
		struct in6_addr dst6;
		int csz;

		if (dst == 0) {
			if (fr_ifpaddr(6, ((qif_t *)fin->fin_qif)->qf_ill,
				       (struct in_addr *)&dst6) == -1)
				return -1;
		} else
			dst6 = oip6->ip6_dst;

		csz = sz;
		sz -= sizeof(ip6_t);
		ip6 = (ip6_t *)mb->b_rptr;
		ip6->ip6_flow = 0;
		ip6->ip6_vfc = 0x60;
		ip6->ip6_hlim = 127;
		ip6->ip6_plen = htons(sz);
		ip6->ip6_nxt = IPPROTO_ICMPV6;
		ip6->ip6_src = dst6;
		ip6->ip6_dst = oip6->ip6_src;
		sz -= offsetof(struct icmp, icmp_ip);
		bcopy((char *)m->b_rptr, (char *)&icmp->icmp_ip, sz);
		icmp->icmp_cksum = csz - sizeof(ip6_t);
	} else
#endif
	{
		ip = (ip_t *)mb->b_rptr;
		ip->ip_v = IPVERSION;
		ip->ip_hl = (sizeof(*ip) >> 2);
		ip->ip_p = IPPROTO_ICMP;
		ip->ip_id = oip->ip_id;
		ip->ip_sum = 0;
		ip->ip_ttl = 60;
		ip->ip_tos = oip->ip_tos;
		ip->ip_len = (u_short)htons(sz);
		if (dst == 0) {
			if (fr_ifpaddr(4, ((qif_t *)fin->fin_qif)->qf_ill,
				       &dst4) == -1)
				return -1;
		} else
			dst4 = oip->ip_dst;
		ip->ip_src = dst4;
		ip->ip_dst = oip->ip_src;
		bcopy((char *)oip, (char *)&icmp->icmp_ip, sizeof(*oip));
		bcopy((char *)oip + (oip->ip_hl << 2),
		      (char *)&icmp->icmp_ip + sizeof(*oip), 8);
		icmp->icmp_cksum = ipf_cksum((u_short *)icmp,
					     sizeof(*icmp) + 8);
	}

	/*
	 * Need to exit out of these so we don't recursively call rw_enter
	 * from fr_qout.
	 */
	return send_ip(fin, mb);
}
