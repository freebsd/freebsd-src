/*
 * Copyright (C) 1993-2000 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#if !defined(lint)
static const char sccsid[] = "@(#)ip_fil.c	2.41 6/5/96 (C) 1993-1995 Darren Reed";
/*static const char rcsid[] = "@(#)$Id: ip_fil.c,v 2.42.2.14 2000/07/18 13:57:55 darrenr Exp $";*/
static const char rcsid[] = "@(#)$FreeBSD$";
#endif

#ifndef	SOLARIS
#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#if defined(KERNEL) && !defined(_KERNEL)
# define	_KERNEL
#endif
#if defined(_KERNEL) && defined(__FreeBSD_version) && \
    (__FreeBSD_version >= 400000) && !defined(KLD_MODULE)
#include "opt_inet6.h"
#endif
#include <sys/param.h>
#if defined(__NetBSD__) && (NetBSD >= 199905) && !defined(IPFILTER_LKM) && \
    defined(_KERNEL)
# include "opt_ipfilter_log.h"
#endif
#if defined(__FreeBSD__) && !defined(__FreeBSD_version)
# if !defined(_KERNEL) || defined(IPFILTER_LKM)
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
#ifdef USE_INET6
# include <netinet/icmp6.h>
#endif
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

#include <machine/in_cksum.h>

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

int	ipl_unreach = ICMP_UNREACH_FILTER;
u_long	ipl_frouteok[2] = {0, 0};

static	int	frzerostats __P((caddr_t));
#if defined(__NetBSD__) || defined(__OpenBSD__) || (__FreeBSD_version >= 300003)
static	int	frrequest __P((int, u_long, caddr_t, int));
#else
static	int	frrequest __P((int, int, caddr_t, int));
#endif
#ifdef	_KERNEL
static	int	(*fr_savep) __P((ip_t *, int, void *, int, struct mbuf **));
static	int	send_ip __P((ip_t *, fr_info_t *, struct mbuf *));
# ifdef	__sgi
extern  kmutex_t        ipf_rw;
extern	KRWLOCK_T	ipf_mutex;
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
int	fr_running = 0;

#if (__FreeBSD_version >= 300000) && defined(_KERNEL)
struct callout_handle ipfr_slowtimer_ch;
#endif
#if defined(__NetBSD__) && (__NetBSD_Version__ >= 104230000)
# include <sys/callout.h>
struct callout ipfr_slowtimer_ch;
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

#if defined(__NetBSD__) || defined(__OpenBSD__)  || \
    (_BSDI_VERSION >= 199701) || (__FreeBSD_version >= 500011)
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
# if defined(__sgi) || (defined(NETBSD_PF) && \
  ((__NetBSD_Version__ >= 104200000) || (__FreeBSD_version >= 500011)))
	int error = 0;
# endif

	SPL_NET(s);
	if (fr_running || (fr_checkp == fr_check)) {
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
#  if (__NetBSD_Version__ >= 104200000) || (__FreeBSD_version >= 500011)
	error = pfil_add_hook((void *)fr_check, PFIL_IN|PFIL_OUT,
			      &inetsw[ip_protox[IPPROTO_IP]].pr_pfh);
	if (error) {
#   ifdef USE_INET6
		goto pfil_error;
#   else
		appr_unload();
		ip_natunload();
		fr_stateunload();
		return error;
#   endif
	}
#  else
	pfil_add_hook((void *)fr_check, PFIL_IN|PFIL_OUT);
#  endif
#  ifdef USE_INET6
	error = pfil_add_hook((void *)fr_check, PFIL_IN|PFIL_OUT,
			      &inetsw[ip_protox[IPPROTO_IPV6]].pr_pfh);
	if (error) {
		pfil_remove_hook((void *)fr_check, PFIL_IN|PFIL_OUT,
				 &inetsw[ip_protox[IPPROTO_IP]].pr_pfh);
pfil_error:
		appr_unload();
		ip_natunload();
		fr_stateunload();
		return error;
	}
#  endif
# endif

# ifdef __sgi
	error = ipfilter_sgi_attach();
	if (error) {
		SPL_X(s);
		appr_unload();
		ip_natunload();
		fr_stateunload();
		return error;
	}
# endif

	bzero((char *)frcache, sizeof(frcache));
	fr_savep = fr_checkp;
	fr_checkp = fr_check;
	fr_running = 1;

	SPL_X(s);
	if (fr_pass & FR_PASS)
		defpass = "pass";
	else if (fr_pass & FR_BLOCK)
		defpass = "block";
	else
		defpass = "no-match -> block";

	printf("%s initialized.  Default = %s all, Logging = %s\n",
		ipfilter_version, defpass,
# ifdef	IPFILTER_LOG
		"enabled");
# else
		"disabled");
# endif
#ifdef  _KERNEL
# if defined(__NetBSD__) && (__NetBSD_Version__ >= 104230000)
	callout_init(&ipfr_slowtimer_ch);
	callout_reset(&ipfr_slowtimer_ch, hz / 2, ipfr_slowtimer, NULL);
# else
#  if (__FreeBSD_version >= 300000) && defined(_KERNEL)
	ipfr_slowtimer_ch = timeout(ipfr_slowtimer, NULL, hz/2);
#  else
	timeout(ipfr_slowtimer, NULL, hz/2);
#  endif
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
#if defined(NETBSD_PF) && \
    ((__NetBSD_Version__ >= 104200000) || (__FreeBSD_version >= 500011))
	int error = 0;
#endif

#ifdef  _KERNEL
# if defined(__NetBSD__) && (__NetBSD_Version__ >= 104230000)
	callout_stop(&ipfr_slowtimer_ch);
# else
#  if (__FreeBSD_version >= 300000)
	untimeout(ipfr_slowtimer, NULL, ipfr_slowtimer_ch);
#  else
#  ifdef __sgi
	untimeout(ipfr_slowtimer);
#   else
	untimeout(ipfr_slowtimer, NULL);
#   endif
#  endif /* FreeBSD */
# endif /* NetBSD */
#endif
	SPL_NET(s);
	if (!fr_running)
	{
		printf("IP Filter: not initialized\n");
		SPL_X(s);
		return 0;
	}

	printf("%s unloaded\n", ipfilter_version);

	fr_checkp = fr_savep;
	i = frflush(IPL_LOGIPF, i);
	fr_running = 0;

# ifdef NETBSD_PF
#  if ((__NetBSD_Version__ >= 104200000) || (__FreeBSD_version >= 500011))
	error = pfil_remove_hook((void *)fr_check, PFIL_IN|PFIL_OUT,
				 &inetsw[ip_protox[IPPROTO_IP]].pr_pfh);
	if (error)
		return error;
#  else
	pfil_remove_hook((void *)fr_check, PFIL_IN|PFIL_OUT);
#  endif
#  ifdef USE_INET6
	error = pfil_remove_hook((void *)fr_check, PFIL_IN|PFIL_OUT,
				 &inetsw[ip_protox[IPPROTO_IPV6]].pr_pfh);
	if (error)
		return error;
#  endif
# endif

# ifdef __sgi
	ipfilter_sgi_detach();
# endif

	appr_unload();
	ipfr_unload();
	ip_natunload();
	fr_stateunload();
	fr_authunload();

	SPL_X(s);
	return 0;
}
#endif /* _KERNEL */


static	int	frzerostats(data)
caddr_t	data;
{
	friostat_t fio;
	int error;

	fr_getstat(&fio);
	error = IWCOPYPTR((caddr_t)&fio, data, sizeof(fio));
	if (error)
		return EFAULT;

	bzero((char *)frstats, sizeof(*frstats) * 2);

	return 0;
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
# if (defined(_KERNEL) && ((_BSDI_VERSION >= 199510) || (BSD >= 199506) || \
       (NetBSD >= 199511) || (__FreeBSD_version >= 220000) || \
       defined(__OpenBSD__)))
, p)
struct proc *p;
# else
)
# endif
dev_t dev;
# if defined(__NetBSD__) || defined(__OpenBSD__) || \
	(_BSDI_VERSION >= 199701) || (__FreeBSD_version >= 300000)
u_long cmd;
# else
int cmd;
# endif
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
		if (fr_running)
			error = nat_ioctl(data, cmd, mode);
		else
			error = EIO;
		SPL_X(s);
		return error;
	}
	if (unit == IPL_LOGSTATE) {
		if (fr_running)
			error = fr_state_ioctl(data, cmd, mode);
		else
			error = EIO;
		SPL_X(s);
		return error;
	}
	if (unit == IPL_LOGAUTH) {
		if (!fr_running)
			return EIO;
		error = fr_auth_ioctl(data, cmd, NULL, NULL);
		SPL_X(s);
		return error;
	}

	switch (cmd) {
	case FIONREAD :
#ifdef IPFILTER_LOG
		error = IWCOPY((caddr_t)&iplused[IPL_LOGIPF], (caddr_t)data,
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
			error = IRCOPY(data, (caddr_t)&enable, sizeof(enable));
			if (error)
				break;
			if (enable)
				error = iplattach();
			else
				error = ipldetach();
		}
		break;
	}
#endif
	case SIOCSETFF :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = IRCOPY(data, (caddr_t)&fr_flags,
				       sizeof(fr_flags));
		break;
	case SIOCGETFF :
		error = IWCOPY((caddr_t)&fr_flags, data, sizeof(fr_flags));
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
		friostat_t	fio;

		fr_getstat(&fio);
		error = IWCOPYPTR((caddr_t)&fio, data, sizeof(fio));
		if (error)
			return EFAULT;
		break;
	}
	case	SIOCFRZST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frzerostats(data);
		break;
	case	SIOCIPFFL :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = IRCOPY(data, (caddr_t)&tmp, sizeof(tmp));
			if (!error) {
				tmp = frflush(unit, tmp);
				error = IWCOPY((caddr_t)&tmp, data,
					       sizeof(tmp));
			}
		}
		break;
	case SIOCSTLCK :
		error = IRCOPY(data, (caddr_t)&tmp, sizeof(tmp));
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
		else
			*(int *)data = ipflog_clear(unit);
		break;
#endif /* IPFILTER_LOG */
	case SIOCGFRST :
		error = IWCOPYPTR((caddr_t)ipfr_fragstats(), data,
				  sizeof(ipfrstat_t));
		if (error)
			return EFAULT;
		break;
	case SIOCAUTHW :
	case SIOCAUTHR :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
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
#ifdef	USE_INET6
	for (f = ipacct6[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipacct6[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter6[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter6[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
#endif
	RWLOCK_EXIT(&ipf_mutex);
	ip_natsync(ifp);
}


static int frrequest(unit, req, data, set)
int unit;
#if defined(__NetBSD__) || defined(__OpenBSD__) || (__FreeBSD_version >= 300003)
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
	u_int   *p, *pp;
	int error = 0, in;
	u_int group;

	fp = &frd;
	error = IRCOPYPTR(data, (caddr_t)fp, sizeof(*fp));
	if (error)
		return EFAULT;
	fp->fr_ref = 0;
#if (BSD >= 199306) && defined(_KERNEL)
	if ((securelevel > 0) && (fp->fr_func != NULL))
		return EPERM;
#endif

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
	else
		return ESRCH;

	if ((group = fp->fr_group)) {
		if (!(fg = fr_findgroup(group, fp->fr_flags, unit, set, NULL)))
			return ESRCH;
		ftail = fprev = fg->fg_start;
	}

	bzero((char *)frcache, sizeof(frcache[0]) * 2);

	if (*fp->fr_ifname) {
		fp->fr_ifa = GETUNIT(fp->fr_ifname, fp->fr_v);
		if (!fp->fr_ifa)
			fp->fr_ifa = (void *)-1;
	}
#if BSD >= 199306
	if (*fp->fr_oifname) {
		fp->fr_oifa = GETUNIT(fp->fr_oifname, fp->fr_v);
		if (!fp->fr_oifa)
			fp->fr_oifa = (void *)-1;
	}
#endif

	fdp = &fp->fr_dif;
	fp->fr_flags &= ~FR_DUP;
	if (*fdp->fd_ifname) {
		fdp->fd_ifp = GETUNIT(fdp->fd_ifname, fp->fr_v);
		if (!fdp->fd_ifp)
			fdp->fd_ifp = (struct ifnet *)-1;
		else
			fp->fr_flags |= FR_DUP;
	}

	fdp = &fp->fr_tif;
	if (*fdp->fd_ifname) {
		fdp->fd_ifp = GETUNIT(fdp->fd_ifname, fp->fr_v);
		if (!fdp->fd_ifp)
			fdp->fd_ifp = (struct ifnet *)-1;
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
		if (!f)
			return ESRCH;
		error = IWCOPYPTR((caddr_t)f, data, sizeof(*f));
		if (error)
			return EFAULT;
		f->fr_hits = 0;
		f->fr_bytes = 0;
		return 0;
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
			if ((f->fr_ref > 1) && f->fr_grp)
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
			f->fr_next = NULL;
			if (f->fr_ref == 0)
				KFREE(f);
		}
	} else {
		if (f)
			error = EEXIST;
		else {
			if (unit == IPL_LOGAUTH)
				return fr_auth_ioctl(data, req, fp, ftail);
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
int send_reset(oip, fin)
struct ip *oip;
fr_info_t *fin;
{
	struct tcphdr *tcp, *tcp2;
	int tlen = 0, hlen;
	struct mbuf *m;
#ifdef	USE_INET6
	ip6_t *ip6, *oip6 = (ip6_t *)oip;
#endif
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
#ifdef	USE_INET6
	hlen = (fin->fin_v == 6) ? sizeof(ip6_t) : sizeof(ip_t);
#else
	hlen = sizeof(ip_t);
#endif
	m->m_len = sizeof(*tcp2) + hlen;
# if	BSD >= 199306
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = m->m_len;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
# endif
	ip = mtod(m, struct ip *);
# ifdef	USE_INET6
	ip6 = (ip6_t *)ip;
# endif
	bzero((char *)ip, sizeof(*tcp2) + hlen);
	tcp2 = (struct tcphdr *)((char *)ip + hlen);

	tcp2->th_sport = tcp->th_dport;
	tcp2->th_dport = tcp->th_sport;
	tcp2->th_ack = ntohl(tcp->th_seq);
	tcp2->th_ack += tlen;
	tcp2->th_ack = htonl(tcp2->th_ack);
	tcp2->th_off = sizeof(*tcp2) >> 2;
	tcp2->th_flags = TH_RST|TH_ACK;
# ifdef	USE_INET6
	if (fin->fin_v == 6) {
		ip6->ip6_plen = htons(sizeof(struct tcphdr));
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_src = oip6->ip6_dst;
		ip6->ip6_dst = oip6->ip6_src;
		tcp2->th_sum = in6_cksum(m, IPPROTO_TCP,
					 sizeof(*ip6), sizeof(*tcp2));
		return send_ip(oip, fin, m);
	}
# endif
	ip->ip_p = IPPROTO_TCP;
	ip->ip_len = htons(sizeof(struct tcphdr));
	ip->ip_src.s_addr = oip->ip_dst.s_addr;
	ip->ip_dst.s_addr = oip->ip_src.s_addr;
	tcp2->th_sum = in_cksum(m, hlen + sizeof(*tcp2));
	ip->ip_len = hlen + sizeof(*tcp2);
	return send_ip(oip, fin, m);
}


static int send_ip(oip, fin, m)
ip_t *oip;
fr_info_t *fin;
struct mbuf *m;
{
	ip_t *ip;

	ip = mtod(m, ip_t *);

	ip->ip_v = fin->fin_v;
	if (ip->ip_v == 4) {
		ip->ip_hl = (sizeof(*oip) >> 2);
		ip->ip_v = IPVERSION;
		ip->ip_tos = oip->ip_tos;
		ip->ip_id = oip->ip_id;
		ip->ip_off = 0;
# if (BSD < 199306) || defined(__sgi)
		ip->ip_ttl = tcp_ttl;
# else
		ip->ip_ttl = ip_defttl;
# endif
		ip->ip_sum = 0;
	}
# ifdef	USE_INET6
	else if (ip->ip_v == 6) {
		ip6_t *ip6 = (ip6_t *)ip;

		ip6->ip6_hlim = 127;

		return ip6_output(m, NULL, NULL, 0, NULL, NULL);
	}
# endif
# ifdef	IPSEC
	m->m_pkthdr.rcvif = NULL;
# endif
	return ipfr_fastroute(m, fin, NULL);
}


int send_icmp_err(oip, type, fin, dst)
ip_t *oip;
int type;
fr_info_t *fin;
int dst;
{
	int err, hlen = 0, xtra = 0, iclen, ohlen = 0, avail, code;
	struct in_addr dst4;
	struct icmp *icmp;
	struct mbuf *m;
	void *ifp;
#ifdef USE_INET6
	ip6_t *ip6, *oip6 = (ip6_t *)oip;
	struct in6_addr dst6;
#endif
	ip_t *ip;

	if ((type < 0) || (type > ICMP_MAXTYPE))
		return -1;

	code = fin->fin_icode;
#ifdef USE_INET6
	if ((code < 0) || (code > sizeof(icmptoicmp6unreach)/sizeof(int)))
		return -1;
#endif

	avail = 0;
	m = NULL;
	ifp = fin->fin_ifp;
	if (fin->fin_v == 4) {
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

# if	(BSD < 199306) || defined(__sgi)
		avail = MLEN;
		m = m_get(M_DONTWAIT, MT_HEADER);
# else
		avail = MHLEN;
		m = m_gethdr(M_DONTWAIT, MT_HEADER);
# endif
		if (m == NULL)
			return ENOBUFS;

		if (dst == 0) {
			if (fr_ifpaddr(4, ifp, &dst4) == -1)
				return -1;
		} else
			dst4.s_addr = oip->ip_dst.s_addr;

		hlen = sizeof(ip_t);
		ohlen = oip->ip_hl << 2;
		xtra = 8;
	}

#ifdef	USE_INET6
	else if (fin->fin_v == 6) {
		hlen = sizeof(ip6_t);
		ohlen = sizeof(ip6_t);
		type = icmptoicmp6types[type];
		if (type == ICMP6_DST_UNREACH)
			code = icmptoicmp6unreach[code];

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (!m)
			return ENOBUFS;

		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			return ENOBUFS;
		}
		avail = (m->m_flags & M_EXT) ? MCLBYTES : MHLEN;
		xtra = MIN(ntohs(oip6->ip6_plen) + sizeof(ip6_t),
			   avail - hlen - sizeof(*icmp) - max_linkhdr);
		if (dst == 0) {
			if (fr_ifpaddr(6, ifp, (struct in_addr *)&dst6) == -1)
				return -1;
		} else
			dst6 = oip6->ip6_dst;
	}
#endif

	iclen = hlen + sizeof(*icmp);
# if	BSD >= 199306
	avail -= (max_linkhdr + iclen);
	m->m_data += max_linkhdr;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
	if (xtra > avail)
		xtra = avail;
	iclen += xtra;
	m->m_pkthdr.len = iclen;
#else
	avail -= (m->m_off + iclen);
	if (xtra > avail)
		xtra = avail;
	iclen += xtra;
#endif
	m->m_len = iclen;
	ip = mtod(m, ip_t *);
	icmp = (struct icmp *)((char *)ip + hlen);
	bzero((char *)ip, iclen);

	icmp->icmp_type = type;
	icmp->icmp_code = fin->fin_icode;
	icmp->icmp_cksum = 0;
	if (avail) {
		bcopy((char *)oip, (char *)&icmp->icmp_ip, MIN(ohlen, avail));
		avail -= MIN(ohlen, avail);
	}

#ifdef	USE_INET6
	ip6 = (ip6_t *)ip;
	if (fin->fin_v == 6) {
		ip6->ip6_flow = 0;
		ip6->ip6_plen = htons(iclen - hlen);
		ip6->ip6_nxt = IPPROTO_ICMPV6;
		ip6->ip6_hlim = 0;
		ip6->ip6_src = dst6;
		ip6->ip6_dst = oip6->ip6_src;
		if (avail)
			bcopy((char *)oip + ohlen,
			      (char *)&icmp->icmp_ip + ohlen, avail);
		icmp->icmp_cksum = in6_cksum(m, IPPROTO_ICMPV6,
					     sizeof(*ip6), iclen - hlen);
	} else
#endif
	{
		ip->ip_src.s_addr = dst4.s_addr;
		ip->ip_dst.s_addr = oip->ip_src.s_addr;

		if (avail > 8)
			avail = 8;
		if (avail)
			bcopy((char *)oip + ohlen,
			      (char *)&icmp->icmp_ip + ohlen, avail);
		icmp->icmp_cksum = ipf_cksum((u_short *)icmp,
					     sizeof(*icmp) + 8);
		ip->ip_len = iclen;
		ip->ip_p = IPPROTO_ICMP;
	}
	err = send_ip(oip, fin, m);
	return err;
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
	int len, off, error = 0, hlen, code;
	struct ifnet *ifp, *sifp;
	struct sockaddr_in *dst;
	struct route iproute;
	frentry_t *fr;

	hlen = fin->fin_hlen;
	ip = mtod(m0, struct ip *);

#ifdef	USE_INET6
	if (ip->ip_v == 6) {
		/*
		 * currently "to <if>" and "to <if>:ip#" are not supported
		 * for IPv6
		 */
		return ip6_output(m0, NULL, NULL, 0, NULL, NULL);
	}
#endif
	/*
	 * Route packet.
	 */
	ro = &iproute;
	bzero((caddr_t)ro, sizeof (*ro));
	dst = (struct sockaddr_in *)&ro->ro_dst;
	dst->sin_family = AF_INET;

	fr = fin->fin_fr;
	if (fdp)
		ifp = fdp->fd_ifp;
	else {
		ifp = fin->fin_ifp;
		dst->sin_addr = ip->ip_dst;
	}

	/*
	 * In case we're here due to "to <if>" being used with "keep state",
	 * check that we're going in the correct direction.
	 */
	if ((fr != NULL) && (fin->fin_rev != 0)) {
		if ((ifp != NULL) && (fdp == &fr->fr_tif))
			return -1;
		dst->sin_addr = ip->ip_dst;
	} else if (fdp)
		dst->sin_addr = fdp->fd_ip.s_addr ? fdp->fd_ip : ip->ip_dst;

# if BSD >= 199306
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
		if (!fr || !(fr->fr_flags & FR_FASTROUTE)) {
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
			ATOMIC_INCL(frstats[1].fr_acct);
		}
		fin->fin_fr = NULL;
		if (!fr || !(fr->fr_flags & FR_RETMASK))
			(void) fr_checkstate(ip, fin);
		(void) ip_natout(ip, fin);
	} else
		ip->ip_sum = 0;
	/*
	 * If small enough for interface, can just send directly.
	 */
	if (ip->ip_len <= ifp->if_mtu) {
# if	BSD >= 199306
		int i = 0;

#  ifdef	MCLISREFERENCED
		if ((m->m_flags & M_EXT) && MCLISREFERENCED(m))
#  else
		if (m->m_flags & M_EXT)
#  endif
			i = 1;
# endif
# ifndef sparc
#  ifndef __FreeBSD__
		ip->ip_id = htons(ip->ip_id);
#  endif
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
# endif
		if (!ip->ip_sum)
			ip->ip_sum = in_cksum(m, hlen);
# if	BSD >= 199306
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst,
					  ro->ro_rt);
		if (i) {
			ip->ip_id = ntohs(ip->ip_id);
			ip->ip_len = ntohs(ip->ip_len);
			ip->ip_off = ntohs(ip->ip_off);
		}
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
# if BSD >= 199306
		m->m_pkthdr.len = mhlen + len;
		m->m_pkthdr.rcvif = NULL;
# endif
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
	if (error == EMSGSIZE) {
		sifp = fin->fin_ifp;
		code = fin->fin_icode;
		fin->fin_icode = ICMP_UNREACH_NEEDFRAG;
		fin->fin_ifp = ifp;
		(void) send_icmp_err(ip, ICMP_UNREACH, fin, 1);
		fin->fin_ifp = sifp;
		fin->fin_icode = code;
	}
	m_freem(m);
	goto done;
}


int fr_verifysrc(ipa, ifp)
struct in_addr ipa;
void *ifp;
{
	struct sockaddr_in *dst;
	struct route iproute;

	bzero((char *)&iproute, sizeof(iproute));
	dst = (struct sockaddr_in *)&iproute.ro_dst;
	dst->sin_family = AF_INET;
	dst->sin_addr = ipa;
# if    (BSD >= 199306) && !defined(__NetBSD__) && !defined(__bsdi__) && \
        !defined(__OpenBSD__)
#  ifdef        RTF_CLONING
	rtalloc_ign(&iproute, RTF_CLONING);
#  else
	rtalloc_ign(&iproute, RTF_PRCLONING);
#  endif
# else
	rtalloc(&iproute);
# endif
	if (iproute.ro_rt == NULL)
		return 0;
	return (ifp == iproute.ro_rt->rt_ifp);
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


struct ifnet *get_unit(name, v)
char *name;
int v;
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
		if (!ifneta)
			return NULL;
		ifneta[1] = NULL;
		ifneta[0] = (struct ifnet *)calloc(1, sizeof(*ifp));
		if (!ifneta[0]) {
			free(ifneta);
			return NULL;
		}
		nifs = 1;
	} else {
		nifs++;
		ifneta = (struct ifnet **)realloc(ifneta,
						  (nifs + 1) * sizeof(*ifa));
		if (!ifneta) {
			nifs = 0;
			return NULL;
		}
		ifneta[nifs] = NULL;
		ifneta[nifs - 1] = (struct ifnet *)malloc(sizeof(*ifp));
		if (!ifneta[nifs - 1]) {
			nifs--;
			return NULL;
		}
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


void frsync()
{
	return;
}
#endif /* _KERNEL */
