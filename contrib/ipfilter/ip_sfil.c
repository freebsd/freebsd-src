/*
 * Copyright (C) 1993-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * I hate legaleese, don't you ?
 */
#if !defined(lint)
static const char sccsid[] = "%W% %G% (C) 1993-1995 Darren Reed";
static const char rcsid[] = "@(#)$Id: ip_sfil.c,v 2.1.2.2 1999/10/05 12:59:08 darrenr Exp $";
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

int	fr_running = 0;
int	ipl_unreach = ICMP_UNREACH_HOST;
u_long	ipl_frouteok[2] = {0, 0};
static	void	frzerostats __P((caddr_t));

static	int	frrequest __P((minor_t, int, caddr_t, int));
kmutex_t	ipl_mutex, ipf_authmx, ipf_rw;
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
	mutex_init(&ipl_mutex, "ipf log mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_rw, "ipf rw mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_authmx, "ipf auth log mutex", MUTEX_DRIVER, NULL);
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


static	void	frzerostats(data)
caddr_t	data;
{
	friostat_t fio;

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

	switch (cmd) {
	case SIOCFRENB :
	{
		u_int	enable;

		if (!(mode & FWRITE))
			error = EPERM;
		else
			IRCOPY((caddr_t)data, (caddr_t)&enable, sizeof(enable));
		break;
	}
	case SIOCSETFF :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			WRITE_ENTER(&ipf_mutex);
			IRCOPY((caddr_t)data, (caddr_t)&fr_flags,
			       sizeof(fr_flags));
			RWLOCK_EXIT(&ipf_mutex);
		}
		break;
	case SIOCGETFF :
		IWCOPY((caddr_t)&fr_flags, (caddr_t)data, sizeof(fr_flags));
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
			IWCOPY((caddr_t)&fr_active, (caddr_t)data,
			       sizeof(fr_active));
			fr_active = 1 - fr_active;
			RWLOCK_EXIT(&ipf_mutex);
		}
		break;
	case SIOCGETFS :
	{
		struct	friostat	fio;

		READ_ENTER(&ipf_mutex);
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
		fio.f_running = fr_running;
		fio.f_groups[0][0] = ipfgroups[0][0];
		fio.f_groups[0][1] = ipfgroups[0][1];
		fio.f_groups[1][0] = ipfgroups[1][0];
		fio.f_groups[1][1] = ipfgroups[1][1];
		fio.f_groups[2][0] = ipfgroups[2][0];
		fio.f_groups[2][1] = ipfgroups[2][1];
#ifdef	IPFILTER_LOG
		fio.f_logging = 1;
#else
		fio.f_logging = 0;
#endif
		fio.f_defpass = fr_pass;
		strncpy(fio.f_version, fio.f_version,
			sizeof(fio.f_version));
		RWLOCK_EXIT(&ipf_mutex);
		IWCOPY((caddr_t)&fio, (caddr_t)data, sizeof(fio));
		break;
	}
	case SIOCFRZST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			frzerostats((caddr_t)data);
		break;
	case	SIOCIPFFL :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			IRCOPY((caddr_t)data, (caddr_t)&tmp, sizeof(tmp));
			tmp = frflush(unit, tmp);
			IWCOPY((caddr_t)&tmp, (caddr_t)data, sizeof(tmp));
		}
		break;
#ifdef	IPFILTER_LOG
	case	SIOCIPFFB :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			tmp = ipflog_clear(unit);
			IWCOPY((caddr_t)&tmp, (caddr_t)data, sizeof(tmp));
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
		IWCOPY((caddr_t)ipfr_fragstats(), (caddr_t)data,
		       sizeof(ipfrstat_t));
		break;
	case FIONREAD :
	{
#ifdef	IPFILTER_LOG
		int copy = (int)iplused[IPL_LOGIPF];

		IWCOPY((caddr_t)&copy, (caddr_t)data, sizeof(copy));
#endif
		break;
	}
	case SIOCAUTHW :
	case SIOCAUTHR :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
	case SIOCATHST :
		error = fr_auth_ioctl((caddr_t)data, cmd, NULL, NULL);
		break;
	default :
		error = EINVAL;
		break;
	}
	RWLOCK_EXIT(&ipf_solaris);
	return error;
}


ill_t	*get_unit(name)
char	*name;
{
	size_t	len = strlen(name) + 1;	/* includes \0 */
	ill_t	*il;

	for (il = ill_g_head; il; il = il->ill_next)
		if ((len == il->ill_name_length) &&
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
	int error = 0, in;
	u_int group;
	ill_t *ill;
	ipif_t *ipif;
	ire_t *ire;

	fp = &fr;
	IRCOPY(data, (caddr_t)fp, sizeof(*fp));
	fp->fr_ref = 0;

	WRITE_ENTER(&ipf_mutex);
	/*
	 * Check that the group number does exist and that if a head group
	 * has been specified, doesn't exist.
	 */
	if ((req != SIOCZRLST) && fp->fr_grhead &&
	    fr_findgroup((u_int)fp->fr_grhead, fp->fr_flags, unit, set, NULL)) {
		error = EEXIST;
		goto out;
	}
	if ((req != SIOCZRLST) && fp->fr_group &&
	    !fr_findgroup((u_int)fp->fr_group, fp->fr_flags, unit, set, NULL)) {
		error = ESRCH;
		goto out;
	}

	in = (fp->fr_flags & FR_INQUE) ? 0 : 1;

	if (unit == IPL_LOGAUTH)
		ftail = fprev = &ipauth;
	else if (fp->fr_flags & FR_ACCOUNT)
		ftail = fprev = &ipacct[in][set];
	else if (fp->fr_flags & (FR_OUTQUE|FR_INQUE))
		ftail = fprev = &ipfilter[in][set];
	else {
		error = ESRCH;
		goto out;
	}

	group = fp->fr_group;
	if (group != NULL) {
		fg = fr_findgroup(group, fp->fr_flags, unit, set, NULL);
		if (fg == NULL) {
			error = ESRCH;
			goto out;
		}
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
		if (!f) {
			error = ESRCH;
			goto out;
		}
		MUTEX_DOWNGRADE(&ipf_mutex);
		IWCOPY((caddr_t)f, data, sizeof(*f));
		f->fr_hits = 0;
		f->fr_bytes = 0;
		goto out;
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
			if (f->fr_ref > 1) {
				error = EBUSY;
				goto out;
			}
			if (fg && fg->fg_head)
				fg->fg_head->fr_ref--;
			if (unit == IPL_LOGAUTH) {
				error = fr_auth_ioctl(data, req, f, ftail);
				goto out;
			}
			if (f->fr_grhead)
				fr_delgroup((u_int)f->fr_grhead, fp->fr_flags,
					    unit, set);
			fixskip(fprev, f, -1);
			*ftail = f->fr_next;
			KFREE(f);
		}
	} else {
		if (f) {
			error = EEXIST;
		} else {
			if (unit == IPL_LOGAUTH) {
				error = fr_auth_ioctl(data, req, f, ftail);
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
				if (group != NULL)
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
int send_reset(fin, iphdr, qif)
fr_info_t *fin;
ip_t *iphdr;
qif_t *qif;
{
	tcphdr_t *tcp, *tcp2;
	int tlen = 0;
	mblk_t *m;
	ip_t *ip;

	tcp = (struct tcphdr *)fin->fin_dp;
	if (tcp->th_flags & TH_RST)
		return -1;
	if (tcp->th_flags & TH_SYN)
		tlen = 1;
	if ((m = (mblk_t *)allocb(sizeof(*ip) + sizeof(*tcp),BPRI_HI)) == NULL)
		return -1;

	MTYPE(m) = M_DATA;
	m->b_wptr += sizeof(*ip) + sizeof(*tcp);
	bzero((char *)m->b_rptr, sizeof(*ip) + sizeof(*tcp));
	ip = (ip_t *)m->b_rptr;
	tcp2 = (struct tcphdr *)(m->b_rptr + sizeof(*ip));

	ip->ip_src.s_addr = iphdr->ip_dst.s_addr;
	ip->ip_dst.s_addr = iphdr->ip_src.s_addr;
	tcp2->th_dport = tcp->th_sport;
	tcp2->th_sport = tcp->th_dport;
	tcp2->th_ack = htonl(ntohl(tcp->th_seq) + tlen);
	tcp2->th_seq = tcp->th_ack;
	tcp2->th_off = sizeof(struct tcphdr) >> 2;
	tcp2->th_flags = TH_RST|TH_ACK;
	/*
	 * This is to get around a bug in the Solaris 2.4/2.5 TCP checksum
	 * computation that is done by their put routine.
	 */
	tcp2->th_sum = htons(0x14);
	ip->ip_hl = sizeof(*ip) >> 2;
	ip->ip_v = IPVERSION;
	ip->ip_p = IPPROTO_TCP;
	ip->ip_len = htons(sizeof(*ip) + sizeof(*tcp));
	ip->ip_tos = iphdr->ip_tos;
	ip->ip_off = 0;
	ip->ip_ttl = 60;
	ip->ip_sum = 0;
	RWLOCK_EXIT(&ipfs_mutex);
	RWLOCK_EXIT(&ipf_solaris);
	ip_wput(qif->qf_ill->ill_wq, m);
	READ_ENTER(&ipf_solaris);
	READ_ENTER(&ipfs_mutex);
	return 0;
}


int	icmp_error(ip, type, code, qif, dst)
ip_t	*ip;
int	type, code;
qif_t	*qif;
struct	in_addr	dst;
{
	mblk_t *mb;
	struct icmp *icmp;
	ip_t *nip;
	u_short sz = sizeof(*nip) + sizeof(*icmp) + 8;

	if ((mb = (mblk_t *)allocb((size_t)sz, BPRI_HI)) == NULL)
		return -1;
	MTYPE(mb) = M_DATA;
	mb->b_wptr += sz;
	bzero((char *)mb->b_rptr, (size_t)sz);
	nip = (ip_t *)mb->b_rptr;
	icmp = (struct icmp *)(nip + 1);

	nip->ip_v = IPVERSION;
	nip->ip_hl = (sizeof(*nip) >> 2);
	nip->ip_p = IPPROTO_ICMP;
	nip->ip_id = ip->ip_id;
	nip->ip_sum = 0;
	nip->ip_ttl = 60;
	nip->ip_tos = ip->ip_tos;
	nip->ip_len = (u_short)htons(sz);
	if (dst.s_addr == 0) {
		if (fr_ifpaddr(qif->qf_ill, &dst) == -1)
			return -1;
	}
	nip->ip_src = dst;
	nip->ip_dst = ip->ip_src;

	icmp->icmp_type = type;
	icmp->icmp_code = code;
	icmp->icmp_cksum = 0;
	bcopy((char *)ip, (char *)&icmp->icmp_ip, sizeof(*ip));
	bcopy((char *)ip + (ip->ip_hl << 2),
	      (char *)&icmp->icmp_ip + sizeof(*ip), 8);	/* 64 bits */
#ifndef	sparc
	ip = &icmp->icmp_ip;
	{
	u_short	__iplen, __ipoff;

	__iplen = ip->ip_len;
	__ipoff = ip->ip_len;
	ip->ip_len = htons(__iplen);
	ip->ip_off = htons(__ipoff);
	}
#endif
	icmp->icmp_cksum = ipf_cksum((u_short *)icmp, sizeof(*icmp) + 8);
	/*
	 * Need to exit out of these so we don't recursively call rw_enter
	 * from fr_qout.
	 */
	RWLOCK_EXIT(&ipfs_mutex);
	RWLOCK_EXIT(&ipf_solaris);
	ip_wput(qif->qf_ill->ill_wq, mb);
	READ_ENTER(&ipf_solaris);
	READ_ENTER(&ipfs_mutex);
	return 0;
}
