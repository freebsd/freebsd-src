/*
 * Copyright (C) 1993-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#if !defined(lint)
static const char sccsid[] = "@(#)ip_fil.c	2.41 6/5/96 (C) 1993-1995 Darren Reed";
static const char rcsid[] = "@(#)$Id: ip_fil.c,v 2.4.2.7 1999/10/15 13:49:43 darrenr Exp $";
#endif

#ifndef	SOLARIS
#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#if defined(KERNEL) && !defined(_KERNEL)
# define	_KERNEL
#endif
#include <sys/param.h>
#if defined(__NetBSD__) && (NetBSD >= 199905) && !defined(IPFILTER_LKM) && \
    defined(_KERNEL)
# include "opt_ipfilter_log.h"
#endif
#ifdef	__FreeBSD__
# if defined(_KERNEL) && !defined(IPFILTER_LKM)
#  include <sys/osreldate.h>
# else
#  include <osreldate.h>
# endif
#endif
#ifndef	_KERNEL
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <ctype.h>
# include <fcntl.h>
#endif
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/file.h>
#if __FreeBSD_version >= 220000 && defined(_KERNEL)
# include <sys/fcntl.h>
# include <sys/filio.h>
#else
# include <sys/ioctl.h>
#endif
#include <sys/time.h>
#ifdef	_KERNEL
# include <sys/systm.h>
#endif
#include <sys/uio.h>
#if !SOLARIS
# if (NetBSD > 199609) || (OpenBSD > 199603) || (__FreeBSD_version >= 300000)
#  include <sys/dirent.h>
# else
#  include <sys/dir.h>
# endif
# include <sys/mbuf.h>
#else
# include <sys/filio.h>
#endif
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#ifdef sun
# include <net/af.h>
#endif
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
# if defined(_KERNEL) && !defined(IPFILTER_LKM)
#  include "opt_ipfilter.h"
# endif
#endif
#ifdef __sgi
#include <sys/debug.h>
# ifdef IFF_DRVRLOCK /* IRIX6 */
#include <sys/hashing.h>
# endif
#endif
#include <net/route.h>
#include <netinet/in.h>
#if !(defined(__sgi) && !defined(IFF_DRVRLOCK)) /* IRIX < 6 */
# include <netinet/in_var.h>
#endif
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#ifndef	_KERNEL
# include <unistd.h>
# include <syslog.h>
#endif
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_auth.h"
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 300000)
# include <sys/malloc.h>
#endif
#ifndef	MIN
# define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif
#if !SOLARIS && defined(_KERNEL) && !defined(__sgi)
# include <sys/kernel.h>
extern	int	ip_optcopy __P((struct ip *, struct ip *));
#endif


extern	struct	protosw	inetsw[];

#ifndef	_KERNEL
# include "ipt.h"
static	struct	ifnet **ifneta = NULL;
static	int	nifs = 0;
#else
# if	(BSD < 199306) || defined(__sgi)
extern	int	tcp_ttl;
# endif
#endif

int	ipl_inited = 0;
int	ipl_unreach = ICMP_UNREACH_FILTER;
u_long	ipl_frouteok[2] = {0, 0};

static	void	frzerostats __P((caddr_t));
#if defined(__NetBSD__) || defined(__OpenBSD__)
static	int	frrequest __P((int, u_long, caddr_t, int));
#else
static	int	frrequest __P((int, int, caddr_t, int));
#endif
#ifdef	_KERNEL
static	int	(*fr_savep) __P((ip_t *, int, void *, int, struct mbuf **));
static	int	send_ip __P((struct mbuf *, ip_t *));
# ifdef	__sgi
extern  kmutex_t        ipf_rw;
# endif
#else
int	ipllog __P((void));
void	init_ifp __P((void));
# ifdef __sgi
static int 	no_output __P((struct ifnet *, struct mbuf *,
			       struct sockaddr *));
static int	write_output __P((struct ifnet *, struct mbuf *,
				  struct sockaddr *));
# else
static int 	no_output __P((struct ifnet *, struct mbuf *,
			       struct sockaddr *, struct rtentry *));
static int	write_output __P((struct ifnet *, struct mbuf *,
				  struct sockaddr *, struct rtentry *));
# endif
#endif
#if defined(IPFILTER_LKM)
int	fr_running = 1;
#else
int	fr_running = 0;
#endif

#if (__FreeBSD_version >= 300000) && defined(_KERNEL)
struct callout_handle ipfr_slowtimer_ch;
#endif

#if (_BSDI_VERSION >= 199510) && defined(_KERNEL)
# include <sys/device.h>
# include <sys/conf.h>

struct cfdriver iplcd = {
	NULL, "ipl", NULL, NULL, DV_DULL, 0
};

struct devsw iplsw = {
	&iplcd,
	iplopen, iplclose, iplread, nowrite, iplioctl, noselect, nommap,
	nostrat, nodump, nopsize, 0,
	nostop
};
#endif /* _BSDI_VERSION >= 199510  && _KERNEL */

#if defined(__NetBSD__) || defined(__OpenBSD__)  || (_BSDI_VERSION >= 199701)
# include <sys/conf.h>
# if defined(NETBSD_PF)
#  include <net/pfil.h>
/*
 * We provide the fr_checkp name just to minimize changes later.
 */
int (*fr_checkp) __P((ip_t *ip, int hlen, void *ifp, int out, mb_t **mp));
# endif /* NETBSD_PF */
#endif /* __NetBSD__ */

#ifdef	_KERNEL
# if	defined(IPFILTER_LKM) && !defined(__sgi)
int iplidentify(s)
char *s;
{
	if (strcmp(s, "ipl") == 0)
		return 1;
	return 0;
}
# endif /* IPFILTER_LKM */


/*
 * Try to detect the case when compiling for NetBSD with pseudo-device
 */
# if defined(__NetBSD__) && defined(PFIL_HOOKS)
void
ipfilterattach(count)
int count;
{
	if (iplattach() != 0)
		printf("IP Filter failed to attach\n");
}
# endif


int iplattach()
{
	char *defpass;
	int s;
# ifdef __sgi
	int error;
# endif

	SPL_NET(s);
	if (ipl_inited || (fr_checkp == fr_check)) {
		printf("IP Filter: already initialized\n");
		SPL_X(s);
		return EBUSY;
	}

# ifdef	IPFILTER_LOG
	ipflog_init();
# endif
	if (nat_init() == -1)
		return -1;
	if (fr_stateinit() == -1)
		return -1;
	if (appr_init() == -1)
		return -1;

# ifdef NETBSD_PF
	pfil_add_hook((void *)fr_check, PFIL_IN|PFIL_OUT);
# endif

# ifdef __sgi
	error = ipfilter_sgi_attach();
	if (error) {
		SPL_X(s);
		return error;
	}
# endif

	ipl_inited = 1;
	bzero((char *)frcache, sizeof(frcache));
	fr_savep = fr_checkp;
	fr_checkp = fr_check;

	SPL_X(s);
	if (fr_pass & FR_PASS)
		defpass = "pass";
	else if (fr_pass & FR_BLOCK)
		defpass = "block";
	else
		defpass = "no-match -> block";

	printf("IP Filter: initialized.  Default = %s all, Logging = %s\n",
		defpass,
# ifdef	IPFILTER_LOG
		"enabled");
# else
		"disabled");
# endif
	printf("%s\n", ipfilter_version);
#ifdef	_KERNEL
# if (__FreeBSD_version >= 300000) && defined(_KERNEL)
	ipfr_slowtimer_ch = timeout(ipfr_slowtimer, NULL, hz/2);
# else
	timeout(ipfr_slowtimer, NULL, hz/2);
# endif
#endif
	return 0;
}


/*
 * Disable the filter by removing the hooks from the IP input/output
 * stream.
 */
int ipldetach()
{
	int s, i = FR_INQUE|FR_OUTQUE;

#ifdef	_KERNEL
# if (__FreeBSD_version >= 300000)
	untimeout(ipfr_slowtimer, NULL, ipfr_slowtimer_ch);
# else
#  ifdef __sgi
	untimeout(ipfr_slowtimer);
#  else
	untimeout(ipfr_slowtimer, NULL);
#  endif
# endif
#endif
	SPL_NET(s);
	if (!ipl_inited)
	{
		printf("IP Filter: not initialized\n");
		SPL_X(s);
		return 0;
	}

	fr_checkp = fr_savep;
	i = frflush(IPL_LOGIPF, i);
	ipl_inited = 0;

# ifdef NETBSD_PF
	pfil_remove_hook((void *)fr_check, PFIL_IN|PFIL_OUT);
# endif

# ifdef __sgi
	ipfilter_sgi_detach();
# endif

	ipfr_unload();
	ip_natunload();
	fr_stateunload();
	fr_authunload();

	SPL_X(s);
	return 0;
}
#endif /* _KERNEL */


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
#ifdef __sgi
int IPL_EXTERN(ioctl)(dev_t dev, int cmd, caddr_t data, int mode
# ifdef _KERNEL
	, cred_t *cp, int *rp
# endif
)
#else
int IPL_EXTERN(ioctl)(dev, cmd, data, mode
#if ((_BSDI_VERSION >= 199510) || (BSD >= 199506) || (NetBSD >= 199511) || \
     (__FreeBSD_version >= 220000) || defined(__OpenBSD__)) && defined(_KERNEL)
, p)
struct proc *p;
#else
)
#endif
dev_t dev;
#if defined(__NetBSD__) || defined(__OpenBSD__) || \
	 (_BSDI_VERSION >= 199701) || (__FreeBSD_version >= 300000)
u_long cmd;
#else
int cmd;
#endif
caddr_t data;
int mode;
#endif /* __sgi */
{
#if defined(_KERNEL) && !SOLARIS
	int s;
#endif
	int error = 0, unit = 0, tmp;

#if (BSD >= 199306) && defined(_KERNEL)
	if ((securelevel >= 2) && (mode & FWRITE))
		return EPERM;
#endif
#ifdef	_KERNEL
	unit = GET_MINOR(dev);
	if ((IPL_LOGMAX < unit) || (unit < 0))
		return ENXIO;
#else
	unit = dev;
#endif

	SPL_NET(s);

	if (unit == IPL_LOGNAT) {
		error = nat_ioctl(data, cmd, mode);
		SPL_X(s);
		return error;
	}
	if (unit == IPL_LOGSTATE) {
		error = fr_state_ioctl(data, cmd, mode);
		SPL_X(s);
		return error;
	}
	switch (cmd) {
	case FIONREAD :
#ifdef IPFILTER_LOG
		IWCOPY((caddr_t)&iplused[IPL_LOGIPF], (caddr_t)data,
		       sizeof(iplused[IPL_LOGIPF]));
#endif
		break;
#if !defined(IPFILTER_LKM) && defined(_KERNEL)
	case SIOCFRENB :
	{
		u_int	enable;

		if (!(mode & FWRITE))
			error = EPERM;
		else {
			IRCOPY(data, (caddr_t)&enable, sizeof(enable));
			if (enable) {
				error = iplattach();
				if (error == 0)
					fr_running = 1;
			} else {
				error = ipldetach();
				if (error == 0)
					fr_running = 0;
			}
		}
		break;
	}
#endif
	case SIOCSETFF :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			IRCOPY(data, (caddr_t)&fr_flags, sizeof(fr_flags));
		break;
	case SIOCGETFF :
		IWCOPY((caddr_t)&fr_flags, data, sizeof(fr_flags));
		break;
	case SIOCINAFR :
	case SIOCRMAFR :
	case SIOCADAFR :
	case SIOCZRLST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, data, fr_active);
		break;
	case SIOCINIFR :
	case SIOCRMIFR :
	case SIOCADIFR :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, data, 1 - fr_active);
		break;
	case SIOCSWAPA :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			bzero((char *)frcache, sizeof(frcache[0]) * 2);
			*(u_int *)data = fr_active;
			fr_active = 1 - fr_active;
		}
		break;
	case SIOCGETFS :
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
		fio.f_auth = ipauth;
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
		strncpy(fio.f_version, ipfilter_version,
			sizeof(fio.f_version));
		IWCOPY((caddr_t)&fio, data, sizeof(fio));
		break;
	}
	case	SIOCFRZST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			frzerostats(data);
		break;
	case	SIOCIPFFL :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			IRCOPY(data, (caddr_t)&tmp, sizeof(tmp));
			tmp = frflush(unit, tmp);
			IWCOPY((caddr_t)&tmp, data, sizeof(tmp));
		}
		break;
#ifdef	IPFILTER_LOG
	case	SIOCIPFFB :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			*(int *)data = ipflog_clear(unit);
		break;
#endif /* IPFILTER_LOG */
	case SIOCGFRST :
		IWCOPY((caddr_t)ipfr_fragstats(), data, sizeof(ipfrstat_t));
		break;
	case SIOCAUTHW :
	case SIOCAUTHR :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
	case SIOCATHST :
		error = fr_auth_ioctl(data, cmd, NULL, NULL);
		break;
	case SIOCFRSYN :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
#if defined(_KERNEL) && defined(__sgi)
			ipfsync();
#endif
			frsync();
		}
		break;
	default :
		error = EINVAL;
		break;
	}
	SPL_X(s);
	return error;
}


void frsync()
{
#ifdef _KERNEL
	register frentry_t *f;
	register struct ifnet *ifp;

# if defined(__OpenBSD__) || ((NetBSD >= 199511) && (NetBSD < 1991011)) || \
     (defined(__FreeBSD_version) && (__FreeBSD_version >= 300000))
#  if (NetBSD >= 199905) || defined(__OpenBSD__)
	for (ifp = ifnet.tqh_first; ifp; ifp = ifp->if_list.tqe_next)
#  else
	for (ifp = ifnet.tqh_first; ifp; ifp = ifp->if_link.tqe_next)
#  endif
# else
	for (ifp = ifnet; ifp; ifp = ifp->if_next)
# endif
		ip_natsync(ifp);

	WRITE_ENTER(&ipf_mutex);
	for (f = ipacct[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == (void *)-1)
			f->fr_ifa = GETUNIT(f->fr_ifname);
	for (f = ipacct[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == (void *)-1)
			f->fr_ifa = GETUNIT(f->fr_ifname);
	for (f = ipfilter[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == (void *)-1)
			f->fr_ifa = GETUNIT(f->fr_ifname);
	for (f = ipfilter[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == (void *)-1)
			f->fr_ifa = GETUNIT(f->fr_ifname);
	RWLOCK_EXIT(&ipf_mutex);
#endif
}


void fr_forgetifp(ifp)
void *ifp;
{
	register frentry_t *f;

	WRITE_ENTER(&ipf_mutex);
	for (f = ipacct[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipacct[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	RWLOCK_EXIT(&ipf_mutex);
	ip_natsync(ifp);
}


static int frrequest(unit, req, data, set)
int unit;
#if defined(__NetBSD__) || defined(__OpenBSD__)
u_long req;
#else
int req;
#endif
int set;
caddr_t data;
{
	register frentry_t *fp, *f, **fprev;
	register frentry_t **ftail;
	frentry_t frd;
	frdest_t *fdp;
	frgroup_t *fg = NULL;
	int error = 0, in;
	u_int group;

	fp = &frd;
	IRCOPY(data, (caddr_t)fp, sizeof(*fp));
	fp->fr_ref = 0;

	/*
	 * Check that the group number does exist and that if a head group
	 * has been specified, doesn't exist.
	 */
	if ((req != SIOCZRLST) && fp->fr_grhead &&
	    fr_findgroup((u_int)fp->fr_grhead, fp->fr_flags, unit, set, NULL))
		return EEXIST;
	if ((req != SIOCZRLST) && fp->fr_group &&
	    !fr_findgroup((u_int)fp->fr_group, fp->fr_flags, unit, set, NULL))
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
		fp->fr_ifa = GETUNIT(fp->fr_ifname);
		if (!fp->fr_ifa)
			fp->fr_ifa = (void *)-1;
	}
#if BSD >= 199306
	if (*fp->fr_oifname) {
		fp->fr_oifa = GETUNIT(fp->fr_oifname);
		if (!fp->fr_oifa)
			fp->fr_oifa = (void *)-1;
	}
#endif

	fdp = &fp->fr_dif;
	fp->fr_flags &= ~FR_DUP;
	if (*fdp->fd_ifname) {
		fdp->fd_ifp = GETUNIT(fdp->fd_ifname);
		if (!fdp->fd_ifp)
			fdp->fd_ifp = (struct ifnet *)-1;
		else
			fp->fr_flags |= FR_DUP;
	}

	fdp = &fp->fr_tif;
	if (*fdp->fd_ifname) {
		fdp->fd_ifp = GETUNIT(fdp->fd_ifname);
		if (!fdp->fd_ifp)
			fdp->fd_ifp = (struct ifnet *)-1;
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
				fr_delgroup((u_int)f->fr_grhead, fp->fr_flags,
					    unit, set);
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
				if ((group = f->fr_grhead))
					fg = fr_addgroup(group, f, unit, set);
			} else
				error = ENOMEM;
		}
	}
	return (error);
}


#ifdef	_KERNEL
/*
 * routines below for saving IP headers to buffer
 */
# ifdef __sgi
#  ifdef _KERNEL
int IPL_EXTERN(open)(dev_t *pdev, int flags, int devtype, cred_t *cp)
#  else
int IPL_EXTERN(open)(dev_t dev, int flags)
#  endif
# else
int IPL_EXTERN(open)(dev, flags
#  if ((_BSDI_VERSION >= 199510) || (BSD >= 199506) || (NetBSD >= 199511) || \
     (__FreeBSD_version >= 220000) || defined(__OpenBSD__)) && defined(_KERNEL)
, devtype, p)
int devtype;
struct proc *p;
#  else
)
#  endif
dev_t dev;
int flags;
# endif /* __sgi */
{
# if defined(__sgi) && defined(_KERNEL)
	u_int min = geteminor(*pdev);
# else
	u_int min = GET_MINOR(dev);
# endif

	if (IPL_LOGMAX < min)
		min = ENXIO;
	else
		min = 0;
	return min;
}


# ifdef __sgi
int IPL_EXTERN(close)(dev_t dev, int flags, int devtype, cred_t *cp)
#else
int IPL_EXTERN(close)(dev, flags
#  if ((_BSDI_VERSION >= 199510) || (BSD >= 199506) || (NetBSD >= 199511) || \
     (__FreeBSD_version >= 220000) || defined(__OpenBSD__)) && defined(_KERNEL)
, devtype, p)
int devtype;
struct proc *p;
#  else
)
#  endif
dev_t dev;
int flags;
# endif /* __sgi */
{
	u_int	min = GET_MINOR(dev);

	if (IPL_LOGMAX < min)
		min = ENXIO;
	else
		min = 0;
	return min;
}

/*
 * iplread/ipllog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
# ifdef __sgi
int IPL_EXTERN(read)(dev_t dev, uio_t *uio, cred_t *crp)
# else
#  if BSD >= 199306
int IPL_EXTERN(read)(dev, uio, ioflag)
int ioflag;
#  else
int IPL_EXTERN(read)(dev, uio)
#  endif
dev_t dev;
register struct uio *uio;
# endif /* __sgi */
{
# ifdef IPFILTER_LOG
	return ipflog_read(GET_MINOR(dev), uio);
# else
	return ENXIO;
# endif
}


/*
 * send_reset - this could conceivably be a call to tcp_respond(), but that
 * requires a large amount of setting up and isn't any more efficient.
 */
int send_reset(fin, oip)
fr_info_t *fin;
struct ip *oip;
{
	struct tcphdr *tcp, *tcp2;
	struct tcpiphdr *tp;
	struct mbuf *m;
	int tlen = 0;
	ip_t *ip;

	tcp = (struct tcphdr *)fin->fin_dp;
	if (tcp->th_flags & TH_RST)
		return -1;		/* feedback loop */
# if	(BSD < 199306) || defined(__sgi)
	m = m_get(M_DONTWAIT, MT_HEADER);
# else
	m = m_gethdr(M_DONTWAIT, MT_HEADER);
# endif
	if (m == NULL)
		return ENOBUFS;
	if (m == NULL)
		return -1;

	if (tcp->th_flags & TH_SYN)
		tlen = 1;
	m->m_len = sizeof(*tcp2) + sizeof(*ip);
# if	BSD >= 199306
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = m->m_len;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
# endif
	bzero(mtod(m, char *), sizeof(struct tcpiphdr));
	ip = mtod(m, struct ip *);
	tp = mtod(m, struct tcpiphdr *);
	tcp2 = (struct tcphdr *)((char *)ip + sizeof(*ip));

	ip->ip_src.s_addr = oip->ip_dst.s_addr;
	ip->ip_dst.s_addr = oip->ip_src.s_addr;
	tcp2->th_dport = tcp->th_sport;
	tcp2->th_sport = tcp->th_dport;
	tcp2->th_ack = ntohl(tcp->th_seq);
	tcp2->th_ack += tlen;
	tcp2->th_ack = htonl(tcp2->th_ack);
	tcp2->th_off = sizeof(*tcp2) >> 2;
	tcp2->th_flags = TH_RST|TH_ACK;
	tp->ti_pr = oip->ip_p;
	tp->ti_len = htons(sizeof(struct tcphdr));
	tcp2->th_sum = in_cksum(m, sizeof(*ip) + sizeof(*tcp2));

	ip->ip_tos = oip->ip_tos;
	ip->ip_p = oip->ip_p;
	ip->ip_len = sizeof(*ip) + sizeof(*tcp2);

	return send_ip(m, ip);
}


static int send_ip(m, ip)
struct mbuf *m;
ip_t *ip;
{
# if (defined(__FreeBSD_version) && (__FreeBSD_version >= 220000)) || \
     (defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802))
	struct route ro;
# endif

# if (BSD < 199306) || defined(__sgi)
	ip->ip_ttl = tcp_ttl;
# else
	ip->ip_ttl = ip_defttl;
# endif

# if defined(__FreeBSD_version) && (__FreeBSD_version >= 220000)
	{
	int err;

	bzero((char *)&ro, sizeof(ro));
	err = ip_output(m, (struct mbuf *)0, &ro, 0, 0);
	if (ro.ro_rt)
		RTFREE(ro.ro_rt);
	return err;
	}
# else
	/*
	 * extra 0 in case of multicast
	 */
#  if _BSDI_VERSION >= 199802
	return ip_output(m, (struct mbuf *)0, &ro, 0, 0, NULL);
#  else
	return ip_output(m, (struct mbuf *)0, 0, 0, 0);
#  endif
# endif
}


int send_icmp_err(oip, type, code, ifp, dst)
ip_t *oip;
int type, code;
void *ifp;
struct in_addr	dst;
{
	struct icmp *icmp;
	struct mbuf *m;
	ip_t *nip;

# if	(BSD < 199306) || defined(__sgi)
	m = m_get(M_DONTWAIT, MT_HEADER);
# else
	m = m_gethdr(M_DONTWAIT, MT_HEADER);
# endif
	if (m == NULL)
		return ENOBUFS;
	m->m_len = sizeof(*nip) + sizeof(*icmp) + 8;
# if	BSD >= 199306
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = sizeof(*nip) + sizeof(*icmp) + 8;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
# endif

	bzero(mtod(m, char *), (size_t)sizeof(*nip) + sizeof(*icmp) + 8);
	nip = mtod(m, ip_t *);
	icmp = (struct icmp *)(nip + 1);

	nip->ip_v = IPVERSION;
	nip->ip_hl = (sizeof(*nip) >> 2);
	nip->ip_p = IPPROTO_ICMP;
	nip->ip_id = oip->ip_id;
	nip->ip_sum = 0;
	nip->ip_ttl = 60;
	nip->ip_tos = oip->ip_tos;
	nip->ip_len = sizeof(*nip) + sizeof(*icmp) + 8;
	if (dst.s_addr == 0) {
		if (fr_ifpaddr(ifp, &dst) == -1)
			return -1;
		dst.s_addr = htonl(dst.s_addr);
	}
	nip->ip_src = dst;
	nip->ip_dst = oip->ip_src;

	icmp->icmp_type = type;
	icmp->icmp_code = code;
	icmp->icmp_cksum = 0;
	bcopy((char *)oip, (char *)&icmp->icmp_ip, sizeof(*oip));
	bcopy((char *)oip + (oip->ip_hl << 2),
	      (char *)&icmp->icmp_ip + sizeof(*oip), 8);	/* 64 bits */
# ifndef	sparc
	{
	register u_short	__iplen, __ipoff;
	ip_t *ip = &icmp->icmp_ip;

	__iplen = ip->ip_len;
	__ipoff = ip->ip_off;
	ip->ip_len = htons(__iplen);
	ip->ip_off = htons(__ipoff);
	}
# endif
	icmp->icmp_cksum = ipf_cksum((u_short *)icmp, sizeof(*icmp) + 8);
	return send_ip(m, nip);
}


# if !defined(IPFILTER_LKM) && (__FreeBSD_version < 300000) && !defined(__sgi)
#  if	(BSD < 199306)
int iplinit __P((void));

int
#  else
void iplinit __P((void));

void
#  endif
iplinit()
{
	if (iplattach() != 0)
		printf("IP Filter failed to attach\n");
	ip_init();
}
# endif /* ! __NetBSD__ */


size_t mbufchainlen(m0)
register struct mbuf *m0;
{
	register size_t len = 0;

	for (; m0; m0 = m0->m_next)
		len += m0->m_len;
	return len;
}


int ipfr_fastroute(m0, fin, fdp)
struct mbuf *m0;
fr_info_t *fin;
frdest_t *fdp;
{
	register struct ip *ip, *mhip;
	register struct mbuf *m = m0;
	register struct route *ro;
	int len, off, error = 0, hlen;
	struct sockaddr_in *dst;
	struct route iproute;
	struct ifnet *ifp;
	frentry_t *fr;

	hlen = fin->fin_hlen;
	ip = mtod(m0, struct ip *);
	/*
	 * Route packet.
	 */
	ro = &iproute;
	bzero((caddr_t)ro, sizeof (*ro));
	dst = (struct sockaddr_in *)&ro->ro_dst;
	dst->sin_family = AF_INET;

	fr = fin->fin_fr;
	ifp = fdp->fd_ifp;
	/*
	 * In case we're here due to "to <if>" being used with "keep state",
	 * check that we're going in the correct direction.
	 */
	if ((fr != NULL) && (ifp != NULL) && (fin->fin_rev != 0) &&
	    (fdp == &fr->fr_tif))
		return -1;
# ifdef	__bsdi__
	dst->sin_len = sizeof(*dst);
# endif
# if	(BSD >= 199306) && !defined(__NetBSD__) && !defined(__bsdi__) && \
	!defined(__OpenBSD__)
#  ifdef	RTF_CLONING
	rtalloc_ign(ro, RTF_CLONING);
#  else
	rtalloc_ign(ro, RTF_PRCLONING);
#  endif
# else
	rtalloc(ro);
# endif
	if (!ifp) {
		if (!(fin->fin_fr->fr_flags & FR_FASTROUTE)) {
			error = -2;
			goto bad;
		}
		if (ro->ro_rt == 0 || (ifp = ro->ro_rt->rt_ifp) == 0) {
			if (in_localaddr(ip->ip_dst))
				error = EHOSTUNREACH;
			else
				error = ENETUNREACH;
			goto bad;
		}
		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = (struct sockaddr_in *)&ro->ro_rt->rt_gateway;
	}
	if (ro->ro_rt)
		ro->ro_rt->rt_use++;

	/*
	 * For input packets which are being "fastrouted", they won't
	 * go back through output filtering and miss their chance to get
	 * NAT'd and counted.
	 */
	fin->fin_ifp = ifp;
	if (fin->fin_out == 0) {
		fin->fin_out = 1;
		if ((fin->fin_fr = ipacct[1][fr_active]) &&
		    (fr_scanlist(FR_NOMATCH, ip, fin, m) & FR_ACCOUNT)) {
			ATOMIC_INC(frstats[1].fr_acct);
		}
		fin->fin_fr = NULL;
		(void) fr_checkstate(ip, fin);
		(void) ip_natout(ip, fin);
	} else
		ip->ip_sum = 0;
	/*
	 * If small enough for interface, can just send directly.
	 */
	if (ip->ip_len <= ifp->if_mtu) {
# ifndef sparc
		ip->ip_id = htons(ip->ip_id);
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
# endif
		if (!ip->ip_sum)
			ip->ip_sum = in_cksum(m, hlen);
# if	BSD >= 199306
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst,
					  ro->ro_rt);
# else
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst);
# endif
		goto done;
	}
	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (ip->ip_off & IP_DF) {
		error = EMSGSIZE;
		goto bad;
	}
	len = (ifp->if_mtu - hlen) &~ 7;
	if (len < 8) {
		error = EMSGSIZE;
		goto bad;
	}

    {
	int mhlen, firstlen = len;
	struct mbuf **mnext = &m->m_act;

	/*
	 * Loop through length of segment after first fragment,
	 * make new header and copy data of each part and link onto chain.
	 */
	m0 = m;
	mhlen = sizeof (struct ip);
	for (off = hlen + len; off < ip->ip_len; off += len) {
# ifdef	MGETHDR
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
# else
		MGET(m, M_DONTWAIT, MT_HEADER);
# endif
		if (m == 0) {
			error = ENOBUFS;
			goto bad;
		}
# if BSD >= 199306
		m->m_data += max_linkhdr;
# else
		m->m_off = MMAXOFF - hlen;
# endif
		mhip = mtod(m, struct ip *);
		bcopy((char *)ip, (char *)mhip, sizeof(*ip));
		if (hlen > sizeof (struct ip)) {
			mhlen = ip_optcopy(ip, mhip) + sizeof (struct ip);
			mhip->ip_hl = mhlen >> 2;
		}
		m->m_len = mhlen;
		mhip->ip_off = ((off - hlen) >> 3) + (ip->ip_off & ~IP_MF);
		if (ip->ip_off & IP_MF)
			mhip->ip_off |= IP_MF;
		if (off + len >= ip->ip_len)
			len = ip->ip_len - off;
		else
			mhip->ip_off |= IP_MF;
		mhip->ip_len = htons((u_short)(len + mhlen));
		m->m_next = m_copy(m0, off, len);
		if (m->m_next == 0) {
			error = ENOBUFS;	/* ??? */
			goto sendorfree;
		}
# ifndef sparc
		mhip->ip_off = htons((u_short)mhip->ip_off);
# endif
		mhip->ip_sum = 0;
		mhip->ip_sum = in_cksum(m, mhlen);
		*mnext = m;
		mnext = &m->m_act;
	}
	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	m_adj(m0, hlen + firstlen - ip->ip_len);
	ip->ip_len = htons((u_short)(hlen + firstlen));
	ip->ip_off = htons((u_short)(ip->ip_off | IP_MF));
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m0, hlen);
sendorfree:
	for (m = m0; m; m = m0) {
		m0 = m->m_act;
		m->m_act = 0;
		if (error == 0)
# if BSD >= 199306
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst, ro->ro_rt);
# else
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst);
# endif
		else
			m_freem(m);
	}
    }	
done:
	if (!error)
		ipl_frouteok[0]++;
	else
		ipl_frouteok[1]++;

	if (ro->ro_rt)
		RTFREE(ro->ro_rt);
	return 0;
bad:
	m_freem(m);
	goto done;
}
#else /* #ifdef _KERNEL */


# ifdef __sgi
static int no_output __P((struct ifnet *ifp, struct mbuf *m,
			   struct sockaddr *s))
# else
static int no_output __P((struct ifnet *ifp, struct mbuf *m,
			   struct sockaddr *s, struct rtentry *rt))
# endif
{
	return 0;
}


# ifdef __STDC__
#  ifdef __sgi
static int write_output __P((struct ifnet *ifp, struct mbuf *m,
			     struct sockaddr *s))
#  else
static int write_output __P((struct ifnet *ifp, struct mbuf *m,
			     struct sockaddr *s, struct rtentry *rt))
#  endif
{
	ip_t *ip = (ip_t *)m;
# else
static int write_output(ifp, ip)
struct ifnet *ifp;
ip_t *ip;
{
# endif
	char fname[32];
	int fd;

# if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
	(defined(OpenBSD) && (OpenBSD >= 199603))
	sprintf(fname, "/tmp/%s", ifp->if_xname);
# else
	sprintf(fname, "/tmp/%s%d", ifp->if_name, ifp->if_unit);
# endif
	fd = open(fname, O_WRONLY|O_APPEND);
	if (fd == -1) {
		perror("open");
		return -1;
	}
	write(fd, (char *)ip, ntohs(ip->ip_len));
	close(fd);
	return 0;
}


struct ifnet *get_unit(name)
char *name;
{
	struct ifnet *ifp, **ifa;
# if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
	(defined(OpenBSD) && (OpenBSD >= 199603))
	for (ifa = ifneta; ifa && (ifp = *ifa); ifa++) {
		if (!strcmp(name, ifp->if_xname))
			return ifp;
	}
# else
	char ifname[32], *s;

	for (ifa = ifneta; ifa && (ifp = *ifa); ifa++) {
		(void) sprintf(ifname, "%s%d", ifp->if_name, ifp->if_unit);
		if (!strcmp(name, ifname))
			return ifp;
	}
# endif

	if (!ifneta) {
		ifneta = (struct ifnet **)malloc(sizeof(ifp) * 2);
		ifneta[1] = NULL;
		ifneta[0] = (struct ifnet *)calloc(1, sizeof(*ifp));
		nifs = 1;
	} else {
		nifs++;
		ifneta = (struct ifnet **)realloc(ifneta,
						  (nifs + 1) * sizeof(*ifa));
		ifneta[nifs] = NULL;
		ifneta[nifs - 1] = (struct ifnet *)malloc(sizeof(*ifp));
	}
	ifp = ifneta[nifs - 1];

# if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
	(defined(OpenBSD) && (OpenBSD >= 199603))
	strncpy(ifp->if_xname, name, sizeof(ifp->if_xname));
# else
	for (s = name; *s && !isdigit(*s); s++)
		;
	if (*s && isdigit(*s)) {
		ifp->if_unit = atoi(s);
		ifp->if_name = (char *)malloc(s - name + 1);
		strncpy(ifp->if_name, name, s - name);
		ifp->if_name[s - name] = '\0';
	} else {
		ifp->if_name = strdup(name);
		ifp->if_unit = -1;
	}
# endif
	ifp->if_output = no_output;
	return ifp;
}



void init_ifp()
{
	struct ifnet *ifp, **ifa;
	char fname[32];
	int fd;

# if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
	(defined(OpenBSD) && (OpenBSD >= 199603))
	for (ifa = ifneta; ifa && (ifp = *ifa); ifa++) {
		ifp->if_output = write_output;
		sprintf(fname, "/tmp/%s", ifp->if_xname);
		fd = open(fname, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC, 0600);
		if (fd == -1)
			perror("open");
		else
			close(fd);
	}
# else

	for (ifa = ifneta; ifa && (ifp = *ifa); ifa++) {
		ifp->if_output = write_output;
		sprintf(fname, "/tmp/%s%d", ifp->if_name, ifp->if_unit);
		fd = open(fname, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC, 0600);
		if (fd == -1)
			perror("open");
		else
			close(fd);
	}
# endif
}


int ipfr_fastroute(ip, fin, fdp)
ip_t *ip;
fr_info_t *fin;
frdest_t *fdp;
{
	struct ifnet *ifp = fdp->fd_ifp;

	if (!ifp)
		return 0;	/* no routing table out here */

	ip->ip_len = htons((u_short)ip->ip_len);
	ip->ip_off = htons((u_short)(ip->ip_off | IP_MF));
	ip->ip_sum = 0;
#ifdef __sgi
	(*ifp->if_output)(ifp, (void *)ip, NULL);
#else
	(*ifp->if_output)(ifp, (void *)ip, NULL, 0);
#endif
	return 0;
}


int ipllog __P((void))
{
	verbose("l");
	return 0;
}


int send_reset(ip, ifp)
ip_t *ip;
struct ifnet *ifp;
{
	verbose("- TCP RST sent\n");
	return 0;
}


int icmp_error(ip, ifp)
ip_t *ip;
struct ifnet *ifp;
{
	verbose("- TCP RST sent\n");
	return 0;
}
#endif /* _KERNEL */
