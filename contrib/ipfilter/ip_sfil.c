/*
 * Copyright (C) 1993-1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * I hate legaleese, don't you ?
 */
#if !defined(lint)
static const char sccsid[] = "%W% %G% (C) 1993-1995 Darren Reed";
static const char rcsid[] = "@(#)$Id: ip_sfil.c,v 2.0.2.25.2.5 1997/12/02 13:55:39 darrenr Exp $";
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
#include "ip_fil.h"
#include "ip_state.h"
#include "ip_nat.h"
#include "ip_frag.h"
#include "ip_auth.h"
#include <inet/ip_ire.h>
#ifndef	MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif

extern	fr_flags, fr_active;

int	ipfr_timer_id = 0;
int	ipl_unreach = ICMP_UNREACH_HOST;
u_long	ipl_frouteok[2] = {0, 0};
static	void	frzerostats __P((caddr_t));

static	int	frrequest __P((int, int, caddr_t, int));
kmutex_t	ipl_mutex, ipf_mutex, ipfs_mutex;
kmutex_t	ipf_frag, ipf_state, ipf_nat, ipf_natfrag, ipf_auth;
kcondvar_t	iplwait, ipfauthwait;


int ipldetach()
{
	int	i;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "ipldetach()\n");
#endif
	for (i = IPL_LOGMAX; i >= 0; i--)
		ipflog_clear(i);
	untimeout(ipfr_timer_id);
	i = FR_INQUE|FR_OUTQUE;
	frflush(IPL_LOGIPF, &i);
	ipfr_unload();
	fr_stateunload();
	ip_natunload();
	cv_destroy(&iplwait);
	cv_destroy(&ipfauthwait);
	mutex_destroy(&ipl_mutex);
	mutex_destroy(&ipf_mutex);
	mutex_destroy(&ipfs_mutex);
	mutex_destroy(&ipf_frag);
	mutex_destroy(&ipf_state);
	mutex_destroy(&ipf_natfrag);
	mutex_destroy(&ipf_nat);
	mutex_destroy(&ipf_auth);
	return 0;
}


int iplattach __P((void))
{
	int	i;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplattach()\n");
#endif
	bzero((char *)nat_table, sizeof(nat_table));
	bzero((char *)frcache, sizeof(frcache));
	mutex_init(&ipl_mutex, "ipf log mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_mutex, "ipf filter mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipfs_mutex, "ipf solaris mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_frag, "ipf fragment mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_state, "ipf IP state mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_nat, "ipf IP NAT mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_natfrag, "ipf IP NAT-Frag mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_auth, "ipf IP User-Auth mutex", MUTEX_DRIVER, NULL);
	cv_init(&iplwait, "ipl condvar", CV_DRIVER, NULL);
	cv_init(&ipfauthwait, "ipf auth condvar", CV_DRIVER, NULL);
	ipfr_timer_id = timeout(ipfr_slowtimer, NULL, drv_usectohz(500000));
	ipflog_init();
	return 0;
}


static	void	frzerostats(data)
caddr_t	data;
{
	struct	friostat	fio;

	bcopy((char *)frstats, (char *)fio.f_st,
		sizeof(struct filterstats) * 2);
	fio.f_fin[0] = ipfilter[0][0];
	fio.f_fin[1] = ipfilter[0][1];
	fio.f_fout[0] = ipfilter[1][0];
	fio.f_fout[1] = ipfilter[1][1];
	fio.f_acctin[0] = ipacct[0][0];
	fio.f_acctin[1] = ipacct[0][1];
	fio.f_acctout[0] = ipacct[1][0];
	fio.f_acctout[1] = ipacct[1][1];
	fio.f_active = fr_active;
	fio.f_froute[0] = ipl_frouteok[0];
	fio.f_froute[1] = ipl_frouteok[1];
	IWCOPY((caddr_t)&fio, data, sizeof(fio));
	bzero((char *)frstats, sizeof(*frstats) * 2);
}


/*
 * Filter ioctl interface.
 */
int iplioctl(dev, cmd, data, mode, cp, rp)
dev_t dev;
int cmd;
int data;
int mode;
cred_t *cp;
int *rp;
{
	int error = 0, unit, tmp;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplioctl(%x,%x,%x,%d,%x,%d)\n",
		dev, cmd, data, mode, cp, rp);
#endif
	unit = getminor(dev);
	if ((IPL_LOGMAX < unit) || (unit < 0))
		return ENXIO;

	if (unit == IPL_LOGNAT) {
		error = nat_ioctl((caddr_t)data, cmd, mode);
		return error;
	}
	if (unit == IPL_LOGSTATE) {
		error = fr_state_ioctl((caddr_t)data, cmd, mode);
		return error;
	}

	switch (cmd) {
	case SIOCFRENB :
	{
		u_int	enable;

		if (!(mode & FWRITE))
			return EPERM;
		IRCOPY((caddr_t)data, (caddr_t)&enable, sizeof(enable));
		break;
	}
	case SIOCSETFF :
		if (!(mode & FWRITE))
			return EPERM;
		mutex_enter(&ipf_mutex);
		IRCOPY((caddr_t)data, (caddr_t)&fr_flags, sizeof(fr_flags));
		mutex_exit(&ipf_mutex);
		break;
	case SIOCGETFF :
		IWCOPY((caddr_t)&fr_flags, (caddr_t)data, sizeof(fr_flags));
		break;
	case SIOCINAFR :
	case SIOCRMAFR :
	case SIOCADAFR :
	case SIOCZRLST :
		if (!(mode & FWRITE))
			return EPERM;
		mutex_enter(&ipf_mutex);
		error = frrequest(unit, cmd, (caddr_t)data, fr_active);
		mutex_exit(&ipf_mutex);
		break;
	case SIOCINIFR :
	case SIOCRMIFR :
	case SIOCADIFR :
		if (!(mode & FWRITE))
			return EPERM;
		mutex_enter(&ipf_mutex);
		error = frrequest(unit, cmd, (caddr_t)data, 1 - fr_active);
		mutex_exit(&ipf_mutex);
		break;
	case SIOCSWAPA :
		if (!(mode & FWRITE))
			return EPERM;
		mutex_enter(&ipf_mutex);
		bzero((char *)frcache, sizeof(frcache[0]) * 2);
		IWCOPY((caddr_t)&fr_active, (caddr_t)data, sizeof(fr_active));
		fr_active = 1 - fr_active;
		mutex_exit(&ipf_mutex);
		break;
	case SIOCGETFS :
	{
		struct	friostat	fio;

		mutex_enter(&ipf_mutex);
		bcopy((char *)frstats, (char *)fio.f_st,
			sizeof(struct filterstats) * 2);
		fio.f_fin[0] = ipfilter[0][0];
		fio.f_fin[1] = ipfilter[0][1];
		fio.f_fout[0] = ipfilter[1][0];
		fio.f_fout[1] = ipfilter[1][1];
		fio.f_acctin[0] = ipacct[0][0];
		fio.f_acctin[1] = ipacct[0][1];
		fio.f_acctout[0] = ipacct[1][0];
		fio.f_acctout[1] = ipacct[1][1];
		fio.f_active = fr_active;
		fio.f_froute[0] = ipl_frouteok[0];
		fio.f_froute[1] = ipl_frouteok[1];
		mutex_exit(&ipf_mutex);
		IWCOPY((caddr_t)&fio, (caddr_t)data, sizeof(fio));
		break;
	}
	case SIOCFRZST :
		if (!(mode & FWRITE))
			return EPERM;
		frzerostats((caddr_t)data);
		break;
	case	SIOCIPFFL :
		if (!(mode & FWRITE))
			return EPERM;
		IRCOPY((caddr_t)data, (caddr_t)&tmp, sizeof(tmp));
		mutex_enter(&ipf_mutex);
		frflush(unit, &tmp);
		mutex_exit(&ipf_mutex);
		IWCOPY((caddr_t)&tmp, (caddr_t)data, sizeof(tmp));
		break;
#ifdef	IPFILTER_LOG
	case	SIOCIPFFB :
		if (!(mode & FWRITE))
			return EPERM;
		tmp = ipflog_clear(unit);
		IWCOPY((caddr_t)&tmp, (caddr_t)data, sizeof(tmp));
		break;
#endif /* IPFILTER_LOG */
	case SIOCFRSYN :
		if (!(mode & FWRITE))
			return EPERM;
		error = ipfsync();
		break;
	case SIOCGFRST :
		IWCOPY((caddr_t)ipfr_fragstats(), (caddr_t)data,
		       sizeof(ipfrstat_t));
		break;
	case FIONREAD :
#ifdef	IPFILTER_LOG
		IWCOPY((caddr_t)&iplused[IPL_LOGIPF], (caddr_t)data,
		       sizeof(iplused[IPL_LOGIPF]));
#endif
		break;
	case SIOCAUTHW :
	case SIOCAUTHR :
		if (!(mode & FWRITE))
			return EPERM;
	case SIOCATHST :
		error = fr_auth_ioctl((caddr_t)data, cmd, NULL, NULL);
		break;
	default :
		error = EINVAL;
		break;
	}
	return error;
}


ill_t	*get_unit(name)
char	*name;
{
	ill_t	*il;
	int	len = strlen(name) + 1;	/* includes \0 */

	for (il = ill_g_head; il; il = il->ill_next)
		if ((len == il->ill_name_length) &&
		    !strncmp(il->ill_name, name, len))
			return il;
	return NULL;
}


static void fixskip(listp, rp, addremove)
frentry_t **listp, *rp;
int addremove;
{
	frentry_t *fp;
	int rules = 0, rn = 0;

	for (fp = *listp; fp && (fp != rp); fp = fp->fr_next, rules++)
		;

	if (!fp)
		return;

	for (fp = *listp; fp && (fp != rp); fp = fp->fr_next, rn++)
		if (fp->fr_skip && (rn + fp->fr_skip >= rules))
			fp->fr_skip += addremove;
}


static int frrequest(unit, req, data, set)
int unit;
int req, set;
caddr_t data;
{
	register frentry_t *fp, *f, **fprev;
	register frentry_t **ftail;
	frentry_t fr;
	frdest_t *fdp;
	frgroup_t *fg = NULL;
	int error = 0, in, group;
	ill_t *ill;
	ipif_t *ipif;
	ire_t *ire;

	fp = &fr;
	IRCOPY(data, (caddr_t)fp, sizeof(*fp));

	/*
	 * Check that the group number does exist and that if a head group
	 * has been specified, doesn't exist.
	 */
	if (fp->fr_grhead &&
	    fr_findgroup(fp->fr_grhead, fp->fr_flags, unit, set, NULL))
		return EEXIST;
	if (fp->fr_group &&
	    !fr_findgroup(fp->fr_group, fp->fr_flags, unit, set, NULL))
		return ESRCH;

	in = (fp->fr_flags & FR_INQUE) ? 0 : 1;

	if (unit == IPL_LOGAUTH)
		ftail = fprev = &ipauth;
	else if (fp->fr_flags & FR_ACCOUNT)
		ftail = fprev = &ipacct[in][set];
	else if (fp->fr_flags & (FR_OUTQUE|FR_INQUE))
		ftail = fprev = &ipfilter[in][set];
	else
		return ESRCH;

	if ((group = fp->fr_group)) {
		if (!(fg = fr_findgroup(group, fp->fr_flags, unit, set, NULL)))
			return ESRCH;
		ftail = fprev = fg->fg_start;
	}

	bzero((char *)frcache, sizeof(frcache[0]) * 2);

	if (*fp->fr_ifname) {
		fp->fr_ifa = (void *)get_unit((char *)fp->fr_ifname);
		if (!fp->fr_ifa)
			fp->fr_ifa = (struct ifnet *)-1;
	}

	fdp = &fp->fr_dif;
	fp->fr_flags &= ~FR_DUP;
	if (*fdp->fd_ifname) {
		ill = get_unit(fdp->fd_ifname);
		if (!ill)
			ire = (ire_t *)-1;
		else if ((ipif = ill->ill_ipif)) {
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
		fdp->fd_ifp = (struct ifnet *)ire;
	}

	fdp = &fp->fr_tif;
	if (*fdp->fd_ifname) {
		ill = get_unit(fdp->fd_ifname);
		if (!ill)
			ire = (ire_t *)-1;
		else if ((ipif = ill->ill_ipif)) {
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
		fdp->fd_ifp = (struct ifnet *)ire;
	}

	/*
	 * Look for a matching filter rule, but don't include the next or
	 * interface pointer in the comparison (fr_next, fr_ifa).
	 */
	for (; (f = *ftail); ftail = &f->fr_next)
		if (bcmp((char *)&f->fr_ip, (char *)&fp->fr_ip,
			 FR_CMPSIZ) == 0)
			break;

	/*
	 * If zero'ing statistics, copy current to caller and zero.
	 */
	if (req == SIOCZRLST) {
		if (!f)
			return ESRCH;
		IWCOPY((caddr_t)f, data, sizeof(*f));
		f->fr_hits = 0;
		f->fr_bytes = 0;
		return 0;
	}

	if (!f) {
		ftail = fprev;
		if (req != SIOCINAFR && req != SIOCINIFR)
			while ((f = *ftail))
				ftail = &f->fr_next;
		else if (fp->fr_hits)
			while (--fp->fr_hits && (f = *ftail))
				ftail = &f->fr_next;
		f = NULL;
	}

	if (req == SIOCDELFR || req == SIOCRMIFR) {
		if (!f)
			error = ESRCH;
		else {
			if (f->fr_ref > 1)
				return EBUSY;
			if (fg && fg->fg_head)
				fg->fg_head->fr_ref--;
			if (unit == IPL_LOGAUTH)
				return fr_auth_ioctl(data, req, f, ftail);
			if (f->fr_grhead)
				fr_delgroup(f->fr_grhead, fp->fr_flags, unit,
					    set);
			fixskip(fprev, f, -1);
			*ftail = f->fr_next;
			KFREE(f);
		}
	} else {
		if (f)
			error = EEXIST;
		else {
			if (unit == IPL_LOGAUTH)
				return fr_auth_ioctl(data, req, f, ftail);
			KMALLOC(f, frentry_t *, sizeof(*f));
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
				if ((group = f->fr_grhead))
					fg = fr_addgroup(group, f, unit, set);
			} else
				error = ENOMEM;
		}
	}
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
	u_int min = getminor(*devp);

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplopen(%x,%x,%x,%x)\n", devp, flags, otype, cred);
#endif
	if (!(otype & OTYP_CHR))
		return ENXIO;
	min = (2 < min || min < 0) ? ENXIO : 0;
	return min;
}


int iplclose(dev, flags, otype, cred)
dev_t dev;
int flags, otype;
cred_t *cred;
{
	u_int	min = getminor(dev);

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplclose(%x,%x,%x,%x)\n", dev, flags, otype, cred);
#endif
	min = (2 < min || min < 0) ? ENXIO : 0;
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
int send_reset(iphdr, qif)
ip_t *iphdr;
qif_t *qif;
{
	struct tcpiphdr *ti = (struct tcpiphdr *)iphdr;
	struct ip *ip;
	struct tcphdr *tcp;
	queue_t *q = qif->qf_q;
	mblk_t *m;
	int tlen = 0;

	if (ti->ti_flags & TH_RST)
		return -1;
	if (ti->ti_flags & TH_SYN)
		tlen = 1;
	if ((m = (mblk_t *)allocb(sizeof(struct tcpiphdr), BPRI_HI)) == NULL)
		return -1;

	MTYPE(m) = M_DATA;
	m->b_wptr += sizeof(struct tcpiphdr);
	bzero((char *)m->b_rptr, sizeof(struct tcpiphdr));
	ip = (ip_t *)m->b_rptr;
	tcp = (struct tcphdr *)(m->b_rptr + sizeof(*ip));

	ip->ip_src.s_addr = ti->ti_dst.s_addr;
	ip->ip_dst.s_addr = ti->ti_src.s_addr;
	tcp->th_dport = ti->ti_sport;
	tcp->th_sport = ti->ti_dport;
	tcp->th_ack = htonl(ntohl(ti->ti_seq) + tlen);
	tcp->th_off = sizeof(struct tcphdr) >> 2;
	tcp->th_flags = TH_RST|TH_ACK;
	/*
	 * This is to get around a bug in the Solaris 2.4/2.5 TCP checksum
	 * computation that is done by their put routine.
	 */
	tcp->th_sum = htons(0x14);
	ip->ip_hl = sizeof(*ip) >> 2;
	ip->ip_v = IPVERSION;
	ip->ip_p = IPPROTO_TCP;
	ip->ip_len = htons(sizeof(struct tcpiphdr));
	ip->ip_tos = ((struct ip *)ti)->ip_tos;
	ip->ip_off = 0;
	ip->ip_ttl = 60;
	ip->ip_sum = 0;
	ip_wput(qif->qf_ill->ill_wq, m);
	return 0;
}


int	icmp_error(ip, type, code, qif, src)
ip_t	*ip;
int	type, code;
qif_t	*qif;
struct	in_addr	src;
{
	queue_t *q = qif->qf_q;
	mblk_t *mb;
	struct icmp *icmp;
	ip_t *nip;
	int sz = sizeof(*nip) + sizeof(*icmp) + 8;

	if ((mb = (mblk_t *)allocb(sz, BPRI_HI)) == NULL)
		return -1;
	MTYPE(mb) = M_DATA;
	mb->b_wptr += sz;
	bzero((char *)mb->b_rptr, sz);
	nip = (ip_t *)mb->b_rptr;
	icmp = (struct icmp *)(nip + 1);

	nip->ip_v = IPVERSION;
	nip->ip_hl = (sizeof(*nip) >> 2);
	nip->ip_p = IPPROTO_ICMP;
	nip->ip_id = ip->ip_id;
	nip->ip_sum = 0;
	nip->ip_ttl = 60;
	nip->ip_tos = ip->ip_tos;
	nip->ip_len = htons(sz);
	nip->ip_src.s_addr = ip->ip_dst.s_addr;
	nip->ip_dst.s_addr = ip->ip_src.s_addr;

	icmp->icmp_type = type;
	icmp->icmp_code = code;
	icmp->icmp_cksum = 0;
	bcopy((char *)ip, (char *)&icmp->icmp_ip, sizeof(*ip));
	bcopy((char *)ip + (ip->ip_hl << 2),
	      (char *)&icmp->icmp_ip + sizeof(*ip), 8);	/* 64 bits */
	icmp->icmp_cksum = ipf_cksum((u_short *)icmp, sizeof(*icmp) + 8);
	ip_wput(qif->qf_ill->ill_wq, mb);
	return 0;
}
