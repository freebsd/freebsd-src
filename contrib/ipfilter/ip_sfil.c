/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * I hate legaleese, don't you ?
 */
#if !defined(lint) && defined(LIBC_SCCS)
static	char	sccsid[] = "%W% %G% (C) 1993-1995 Darren Reed";
static	char	rcsid[] = "$Id: ip_sfil.c,v 2.0.2.8 1997/05/24 07:42:56 darrenr Exp $";
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
#include <inet/ip_ire.h>
#ifndef	MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif

extern	fr_flags, fr_active;

int	ipfr_timer_id = 0;
int	ipl_unreach = ICMP_UNREACH_HOST;
int	send_reset __P((struct tcpiphdr *, qif_t *, queue_t *));
u_short	ipf_cksum __P((u_short *, int));
static	void	frzerostats __P((caddr_t));

#ifdef	IPFILTER_LOG
int	ipllog __P((u_int, int, ip_t *, fr_info_t *, mblk_t *));
static	void	frflush __P((caddr_t));
char	iplbuf[3][IPLLOGSIZE];
caddr_t	iplh[3], iplt[3];
int	iplused[3] = {0, 0, 0};
#endif /* IPFILTER_LOG */
static	int	frrequest __P((int, caddr_t, int));
kmutex_t	ipl_mutex, ipf_mutex, ipfs_mutex;
kmutex_t	ipf_frag, ipf_state, ipf_nat, ipf_natfrag;
kcondvar_t	iplwait;


int ipldetach()
{
	int	i = FR_INQUE|FR_OUTQUE;

	untimeout(ipfr_timer_id);
	frflush((caddr_t)&i);
	ipfr_unload();
	fr_stateunload();
	ip_natunload();
	cv_destroy(&iplwait);
	mutex_destroy(&ipl_mutex);
	mutex_destroy(&ipf_mutex);
	mutex_destroy(&ipfs_mutex);
	mutex_destroy(&ipf_frag);
	mutex_destroy(&ipf_state);
	mutex_destroy(&ipf_natfrag);
	mutex_destroy(&ipf_nat);
	return 0;
}


int iplattach __P((void))
{
	int	i;

	for (i = 0; i <= 2; i++) {
		iplt[i] = iplbuf[i];
		iplh[i] = iplbuf[i];
	}

	bzero((char *)nat_table, sizeof(nat_t *) * NAT_SIZE * 2);
	mutex_init(&ipl_mutex, "ipf log mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_mutex, "ipf filter mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipfs_mutex, "ipf solaris mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_frag, "ipf fragment mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_state, "ipf IP state mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_nat, "ipf IP NAT mutex", MUTEX_DRIVER, NULL);
	mutex_init(&ipf_natfrag, "ipf IP NAT-Frag mutex", MUTEX_DRIVER, NULL);
	cv_init(&iplwait, "ipl condvar", CV_DRIVER, NULL);
	ipfr_timer_id = timeout(ipfr_slowtimer, NULL, drv_usectohz(500000));
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
	IWCOPY((caddr_t)&fio, data, sizeof(fio));
	bzero((char *)frstats, sizeof(*frstats) * 2);
}


#ifdef	IPFILTER_LOG
static void frflush(data)
caddr_t data;
{
	struct frentry *f, **fp;
	int flags, flushed = 0, set = fr_active;

	IRCOPY(data, (caddr_t)&flags, sizeof(flags));
	bzero((char *)frcache, sizeof(frcache[0]) * 2);

	if (flags & FR_INACTIVE)
		set = 1 - set;
	if (flags & FR_OUTQUE) {
		for (fp = &ipfilter[1][set]; (f = *fp); ) {
			*fp = f->fr_next;
			KFREE(f);
			flushed++;
		}
		for (fp = &ipacct[1][set]; (f = *fp); ) {
			*fp = f->fr_next;
			KFREE(f);
			flushed++;
		}
	}
	if (flags & FR_INQUE) {
		for (fp = &ipfilter[0][set]; (f = *fp); ) {
			*fp = f->fr_next;
			KFREE(f);
			flushed++;
		}
		for (fp = &ipacct[0][set]; (f = *fp); ) {
			*fp = f->fr_next;
			KFREE(f);
			flushed++;
		}
	}

	IWCOPY((caddr_t)&flushed, data, sizeof(flushed));
}
#endif /* IPFILTER_LOG */


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
	int error = 0, unit;

	unit = getminor(dev);
        if ((2 < unit) || (unit < 0))
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
		error = frrequest(cmd, (caddr_t)data, fr_active);
		mutex_exit(&ipf_mutex);
		break;
	case SIOCINIFR :
	case SIOCRMIFR :
	case SIOCADIFR :
		if (!(mode & FWRITE))
			return EPERM;
		mutex_enter(&ipf_mutex);
		error = frrequest(cmd, (caddr_t)data, 1 - fr_active);
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
		mutex_exit(&ipf_mutex);
		IWCOPY((caddr_t)&fio, (caddr_t)data, sizeof(fio));
		break;
	}
	case SIOCFRZST :
		if (!(mode & FWRITE))
			return EPERM;
		frzerostats((caddr_t)data);
		break;
#ifdef	IPFILTER_LOG
	case	SIOCIPFFL :
		if (!(mode & FWRITE))
			return EPERM;
		mutex_enter(&ipf_mutex);
		frflush((caddr_t)data);
		mutex_exit(&ipf_mutex);
		break;
	case	SIOCIPFFB :
		if (!(mode & FWRITE))
			return EPERM;
		mutex_enter(&ipl_mutex);
		IWCOPY((caddr_t)&iplused[unit], (caddr_t)data,
		       sizeof(iplused[unit]));
		iplh[unit] = iplt[unit] = iplbuf[unit];
		iplused[unit] = 0;
		mutex_exit(&ipl_mutex);
		break;
#endif /* IPFILTER_LOG */
	case SIOCFRSYN :
		if (!(mode & FWRITE))
			return EPERM;
		error = ipfsync();
		break;
	case SIOCADNAT :
	case SIOCRMNAT :
	case SIOCGNATS :
	case SIOCGNATL :
	case SIOCFLNAT :
	case SIOCCNATL :
		error = nat_ioctl((caddr_t)data, cmd, mode);
		break;
	case SIOCGFRST :
		IWCOPY((caddr_t)ipfr_fragstats(), (caddr_t)data,
		       sizeof(ipfrstat_t));
		break;
	case SIOCGIPST :   
		IWCOPY((caddr_t)fr_statetstats(), (caddr_t)data,
		       sizeof(ips_stat_t));
		break;
	case FIONREAD :
#ifdef	IPFILTER_LOG
		*(int *)data = iplused[IPL_LOGIPF];
#endif
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


static int frrequest(req, data, set)
int req, set;
caddr_t data;
{
	register frentry_t *fp, *f, **fprev;
	register frentry_t **ftail;
	frentry_t fr;
	frdest_t *fdp;
	int error = 0, in;
	ill_t *ill;
	ipif_t *ipif;
	ire_t *ire;

	fp = &fr;
	IRCOPY(data, (caddr_t)fp, sizeof(*fp));

	bzero((char *)frcache, sizeof(frcache[0]) * 2);

	in = (fp->fr_flags & FR_INQUE) ? 0 : 1;
	if (fp->fr_flags & FR_ACCOUNT)
		ftail = fprev = &ipacct[in][set];
	else if (fp->fr_flags & (FR_OUTQUE|FR_INQUE))
		ftail = fprev = &ipfilter[in][set];
	else
		return ESRCH;

	if (*fp->fr_ifname) {
		fp->fr_ifa = (struct ifnet *)get_unit((char *)fp->fr_ifname);
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
			ire = ipif_to_ire(ipif);
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
			ire = ipif_to_ire(ipif);
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
			*ftail = f->fr_next;
			KFREE(f);
		}
	} else {
		if (f)
			error = EEXIST;
		else {
			KMALLOC(f, frentry_t *, sizeof(*f));
			if (f != NULL) {
				bcopy((char *)fp, (char *)f, sizeof(*f));
				f->fr_hits = 0;
				f->fr_next = *ftail;
				*ftail = f;
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
	register int ret;
	register size_t sz, sx;
	char *h, *t;
	int error, used, usedo, copied, unit;

	unit = getminor(dev);

	if (!uio->uio_resid)
		return 0;
	if ((uio->uio_resid < 0) || (uio->uio_resid > IPLLOGSIZE))
		return EINVAL;

	/*
	 * Lock the log so we can snapshot the variables.  Wait for a signal
	 * if the log is empty.
	 */
	mutex_enter(&ipl_mutex);
	while (!iplused[unit]) {
		error = cv_wait_sig(&iplwait, &ipl_mutex);
		if (!error) {
			mutex_exit(&ipl_mutex);
			return EINTR;
		}
	}
	h = iplh[unit];
	t = iplt[unit];
	used = iplused[unit];
	mutex_exit(&ipl_mutex);
	usedo = used;

	/*
	 * Given up the mutex, the log can grow more, but we can't hold the
	 * mutex across the uiomove's.
	 */
	sx = sz = MIN(uio->uio_resid, used);
	if (h <= t)
		sz = MIN(sz, IPLLOGSIZE + iplbuf[unit] - t);

	if (!(ret = uiomove(t, sz, UIO_READ, uio))) {
		t += sz;
		sx -= sz;
		used -= sz;
		if ((h < t) && (t >= iplbuf[unit] + IPLLOGSIZE))
			t = iplbuf[unit];

		if (sx && !(ret = uiomove(t, sx, UIO_READ, uio)))
			used -= sx;
	}

	/*
	 * copied all the data, now adjust variables to match this.
	 */
	mutex_enter(&ipl_mutex);
	copied = usedo - used;
	iplused[unit] -= copied;

	if (!iplused[unit])	/* minimise wrapping around the end */
		iplh[unit] = iplt[unit] = iplbuf[unit];
	else {
		iplt[unit] += copied;
		if (iplt[unit] >= iplbuf[unit] + IPLLOGSIZE)
			iplt[unit] -= IPLLOGSIZE;
		if (iplt[unit] == iplbuf[unit] + IPLLOGSIZE)
			iplt[unit] = iplbuf[unit];
	}
	mutex_exit(&ipl_mutex);
	return ret;
}


int ipllog(flags, dev, ip, fin, m)
u_int flags;
int dev;
ip_t *ip;
fr_info_t *fin;
mblk_t *m;
{
	struct ipl_ci iplci;
	register int len, mlen, hlen;
	register u_char *s = (u_char *)ip;
	ill_t *il = fin->fin_ifp;

	hlen = fin->fin_hlen;
	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
		hlen += MIN(sizeof(tcphdr_t), fin->fin_dlen);
	else if (ip->ip_p == IPPROTO_ICMP) {
		struct	icmp	*icmp = (struct icmp *)(s + hlen);

		switch (icmp->icmp_type) {
		case ICMP_UNREACH :
		case ICMP_SOURCEQUENCH :
		case ICMP_REDIRECT :
		case ICMP_TIMXCEED :
		case ICMP_PARAMPROB :
			hlen += MIN(sizeof(struct icmp) + 8, fin->fin_dlen);
			break;
		default :
			hlen += MIN(sizeof(struct icmp), fin->fin_dlen);
			break;
		}
	}

	mlen = (flags & FR_LOGBODY) ? MIN(msgdsize(m) - hlen, 128) : 0;
	len = hlen + sizeof(iplci) + mlen;
	mutex_enter(&ipl_mutex);
	if ((iplused[dev] + len) > IPLLOGSIZE) {
		mutex_exit(&ipl_mutex);
		return 0;
	}
	iplused[dev] += len;

	uniqtime((struct timeval *)&iplci);
	iplci.flags = flags;
	iplci.hlen = (u_char)hlen;
	iplci.plen = (u_char)mlen;
	iplci.rule = fin->fin_rule;
	iplci.unit = (u_char)il->ill_ppa;
	bcopy(il->ill_name, (char *)iplci.ifname, MIN(il->ill_name_length, 4));

	/*
	 * Gauranteed to succeed from above
	 */
	(void) fr_copytolog(dev, (char *)&iplci, sizeof(iplci));
	len -= sizeof(iplci);

	if (len && m) {
		s = m->b_rptr;
		do {
			if ((hlen = MIN(m->b_wptr - s, len))) {
				if (fr_copytolog(dev, s, hlen))
					break;
				len -= hlen;
			}
			if ((m = m->b_cont))
				s = m->b_rptr;
		} while (m && len);
	}

	cv_signal(&iplwait);
	mutex_exit(&ipl_mutex);
	return 1;
}
#endif /* IPFILTER_LOG */


/*
 * send_reset - this could conceivably be a call to tcp_respond(), but that
 * requires a large amount of setting up and isn't any more efficient.
 */
int send_reset(ti, qif, q)
struct tcpiphdr *ti;
qif_t *qif;
queue_t *q;
{
	struct ip *ip;
	struct tcphdr *tcp;
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


int	icmp_error(q, ip, type, code, qif, src)
queue_t	*q;
ip_t	*ip;
int	type, code;
qif_t	*qif;
struct	in_addr	src;
{
	mblk_t	*mb;
	struct	icmp	*icmp;
	ip_t	*nip;
	int	sz = sizeof(*nip) + sizeof(*icmp) + 8;

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
