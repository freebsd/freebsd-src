/*
 * Copyright (C) 1997-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: ip_log.c,v 2.5.2.1 2000/07/19 13:11:47 darrenr Exp $
 * $FreeBSD$
 */
#include <sys/param.h>
#if defined(KERNEL) && !defined(_KERNEL)
# define       _KERNEL
#endif
#if defined(__NetBSD__) && (NetBSD >= 199905) && !defined(IPFILTER_LKM) && \
    defined(_KERNEL)
# include "opt_ipfilter_log.h"
#endif
#ifdef  __FreeBSD__
# if defined(IPFILTER_LKM) || defined(_KERNEL)
#  if !defined(__FreeBSD_version) 
#   include <sys/osreldate.h>
#  endif
#  if !defined(IPFILTER_LKM)
#   if defined(__FreeBSD_version) && (__FreeBSD_version >= 300000)
#    include "opt_ipfilter.h"
#   endif
#  endif
# else
#  ifdef KLD_MODULE
#   ifndef __FreeBSD_cc_version
#    include <osreldate.h>
#   else
#    if __FreeBSD_cc_version < 430000
#     include <osreldate.h>
#    endif
#   endif
#  endif
# endif
#endif
#ifdef  IPFILTER_LOG
# ifndef SOLARIS
#  define SOLARIS (defined(sun) && (defined(__svr4__) || defined(__SVR4)))
# endif
# ifndef _KERNEL
#  include <stdio.h>
#  include <string.h>
#  include <stdlib.h>
#  include <ctype.h>
# endif
# include <sys/errno.h>
# include <sys/types.h>
# include <sys/file.h>
# if __FreeBSD_version >= 220000 && defined(_KERNEL)
#  include <sys/fcntl.h>
#  include <sys/filio.h>
# else
#  include <sys/ioctl.h>
# endif
# include <sys/time.h>
# if defined(_KERNEL)
#  include <sys/systm.h>
# endif
# if !SOLARIS
#  if (NetBSD > 199609) || (OpenBSD > 199603) || (__FreeBSD_version >= 300000)
#   include <sys/dirent.h>
#  else
#   include <sys/dir.h>
#  endif
#  include <sys/mbuf.h>
# else
#  include <sys/filio.h>
#  include <sys/cred.h>
#  include <sys/kmem.h>
#  ifdef _KERNEL
#   include <sys/ddi.h>
#   include <sys/sunddi.h>
#   include <sys/ksynch.h>
#   include <sys/dditypes.h>
#   include <sys/cmn_err.h>
#  endif
# endif
# include <sys/protosw.h>
# include <sys/socket.h>

# include <net/if.h>
# ifdef sun
#  include <net/af.h>
# endif
# if __FreeBSD_version >= 300000
#  include <net/if_var.h>
# endif
# include <net/route.h>
# include <netinet/in.h>
# ifdef __sgi
#  define _KMEMUSER
#  include <sys/ddi.h>
#  ifdef IFF_DRVRLOCK /* IRIX6 */
#   include <sys/hashing.h>
#  endif
# endif
# if !(defined(__sgi) && !defined(IFF_DRVRLOCK)) /*IRIX<6*/
#  include <netinet/in_var.h>
# endif
# include <netinet/in_systm.h>
# include <netinet/ip.h>
# include <netinet/tcp.h>
# include <netinet/udp.h>
# include <netinet/ip_icmp.h>
# ifdef USE_INET6
#  include <netinet/icmp6.h>
# endif
# include <netinet/ip_var.h>
# ifndef _KERNEL
#  include <syslog.h>
# endif
# include "netinet/ip_compat.h"
# include <netinet/tcpip.h>
# include "netinet/ip_fil.h"
# if (__FreeBSD_version >= 300000)
#  include <sys/malloc.h>
# endif

# ifndef MIN
#  define	MIN(a,b)	(((a)<(b))?(a):(b))
# endif
# ifdef IPFILTER_LOGSIZE
#  undef IPLLOGSIZE
#  define IPLLOGSIZE IPFILTER_LOGSIZE
# endif


# if SOLARIS || defined(__sgi)
extern	kmutex_t	ipl_mutex;
#  if SOLARIS
extern	kcondvar_t	iplwait;
#  endif
# endif

iplog_t	**iplh[IPL_LOGMAX+1], *iplt[IPL_LOGMAX+1], *ipll[IPL_LOGMAX+1];
size_t	iplused[IPL_LOGMAX+1];
static fr_info_t	iplcrc[IPL_LOGMAX+1];


/*
 * Initialise log buffers & pointers.  Also iniialised the CRC to a local
 * secret for use in calculating the "last log checksum".
 */
void ipflog_init()
{
	int	i;

	for (i = IPL_LOGMAX; i >= 0; i--) {
		iplt[i] = NULL;
		ipll[i] = NULL;
		iplh[i] = &iplt[i];
		iplused[i] = 0;
		bzero((char *)&iplcrc[i], sizeof(iplcrc[i]));
	}
}


/*
 * ipflog
 * Create a log record for a packet given that it has been triggered by a
 * rule (or the default setting).  Calculate the transport protocol header
 * size using predetermined size of a couple of popular protocols and thus
 * how much data to copy into the log, including part of the data body if
 * requested.
 */
int ipflog(flags, ip, fin, m)
u_int flags;
ip_t *ip;
fr_info_t *fin;
mb_t *m;
{
	ipflog_t ipfl;
	register size_t mlen, hlen;
	size_t sizes[2];
	void *ptrs[2];
	int types[2];
	u_char p;
# if SOLARIS && defined(_KERNEL)
	ill_t *ifp = fin->fin_ifp;
# else
	struct ifnet *ifp = fin->fin_ifp;
# endif

	/*
	 * calculate header size.
	 */
	hlen = fin->fin_hlen;
	if (fin->fin_off == 0) {
		p = fin->fin_fi.fi_p;
		if (p == IPPROTO_TCP)
			hlen += MIN(sizeof(tcphdr_t), fin->fin_dlen);
		else if (p == IPPROTO_UDP)
			hlen += MIN(sizeof(udphdr_t), fin->fin_dlen);
		else if (p == IPPROTO_ICMP) {
			struct icmp *icmp;

			icmp = (struct icmp *)fin->fin_dp;
	 
			/*
			 * For ICMP, if the packet is an error packet, also
			 * include the information about the packet which
			 * caused the error.
			 */
			switch (icmp->icmp_type)
			{
			case ICMP_UNREACH :
			case ICMP_SOURCEQUENCH :
			case ICMP_REDIRECT :
			case ICMP_TIMXCEED :
			case ICMP_PARAMPROB :
				hlen += MIN(sizeof(struct icmp) + 8,
					    fin->fin_dlen);
				break;
			default :
				hlen += MIN(sizeof(struct icmp),
					    fin->fin_dlen);
				break;
			}
		}
#ifdef USE_INET6
		else if (p == IPPROTO_ICMPV6) {
			struct icmp6_hdr *icmp;

			icmp = (struct icmp6_hdr *)fin->fin_dp;
	 
			/*
			 * For ICMPV6, if the packet is an error packet, also
			 * include the information about the packet which
			 * caused the error.
			 */
			if (icmp->icmp6_type < 128) {
				hlen += MIN(sizeof(struct icmp6_hdr) + 8,
					    fin->fin_dlen);
			} else {
				hlen += MIN(sizeof(struct icmp6_hdr),
					    fin->fin_dlen);
			}
		}
#endif
	}
	/*
	 * Get the interface number and name to which this packet is
	 * currently associated.
	 */
	bzero((char *)ipfl.fl_ifname, sizeof(ipfl.fl_ifname));
# if SOLARIS && defined(_KERNEL)
	ipfl.fl_unit = (u_char)ifp->ill_ppa;
	bcopy(ifp->ill_name, ipfl.fl_ifname,
	      MIN(ifp->ill_name_length, sizeof(ipfl.fl_ifname)));
	mlen = (flags & FR_LOGBODY) ? MIN(msgdsize(m) - hlen, 128) : 0;
# else
#  if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199603)) || \
	(defined(OpenBSD) && (OpenBSD >= 199603) || \
	(defined(__FreeBSD__) && (__FreeBSD_version >= 501113)) )
	strncpy(ipfl.fl_ifname, ifp->if_xname, IFNAMSIZ);
#  else
	ipfl.fl_unit = (u_char)ifp->if_unit;
	strncpy(ipfl.fl_ifname, ifp->if_name, MIN(sizeof(ipfl.fl_ifname),
						  sizeof(ifp->if_name)));
#  endif
	mlen = (flags & FR_LOGBODY) ? MIN(fin->fin_plen - hlen, 128) : 0;
# endif
	ipfl.fl_plen = (u_char)mlen;
	ipfl.fl_hlen = (u_char)hlen;
	ipfl.fl_rule = fin->fin_rule;
	ipfl.fl_group = fin->fin_group;
	if (fin->fin_fr != NULL)
		ipfl.fl_loglevel = fin->fin_fr->fr_loglevel;
	else
		ipfl.fl_loglevel = 0xffff;
	ipfl.fl_flags = flags;
	ipfl.fl_dir = fin->fin_out;
	ptrs[0] = (void *)&ipfl;
	sizes[0] = sizeof(ipfl);
	types[0] = 0;
# if SOLARIS && defined(_KERNEL)
	/*
	 * Are we copied from the mblk or an aligned array ?
	 */
	if (ip == (ip_t *)m->b_rptr) {
		ptrs[1] = m;
		sizes[1] = hlen + mlen;
		types[1] = 1;
	} else {
		ptrs[1] = ip;
		sizes[1] = hlen + mlen;
		types[1] = 0;
	}
# else
	ptrs[1] = m;
	sizes[1] = hlen + mlen;
	types[1] = 1;
# endif
	return ipllog(IPL_LOGIPF, fin, ptrs, sizes, types, 2);
}


/*
 * ipllog
 */
int ipllog(dev, fin, items, itemsz, types, cnt)
int dev;
fr_info_t *fin;
void **items;
size_t *itemsz;
int *types, cnt;
{
	caddr_t buf, s;
	iplog_t *ipl;
	size_t len;
	int i;
 
	/*
	 * Check to see if this log record has a CRC which matches the last
	 * record logged.  If it does, just up the count on the previous one
	 * rather than create a new one.
	 */
	MUTEX_ENTER(&ipl_mutex);
	if (fin != NULL) {
		if ((ipll[dev] != NULL) &&
		    bcmp((char *)fin, (char *)&iplcrc[dev], FI_LCSIZE) == 0) {
			ipll[dev]->ipl_count++;
			MUTEX_EXIT(&ipl_mutex);
			return 1;
		}
		bcopy((char *)fin, (char *)&iplcrc[dev], FI_LCSIZE);
	} else
		bzero((char *)&iplcrc[dev], FI_LCSIZE);
	MUTEX_EXIT(&ipl_mutex);

	/*
	 * Get the total amount of data to be logged.
	 */
	for (i = 0, len = IPLOG_SIZE; i < cnt; i++)
		len += itemsz[i];

	/*
	 * check that we have space to record this information and can
	 * allocate that much.
	 */
	KMALLOCS(buf, caddr_t, len);
	if (!buf)
		return 0;
	MUTEX_ENTER(&ipl_mutex);
	if ((iplused[dev] + len) > IPLLOGSIZE) {
		MUTEX_EXIT(&ipl_mutex);
		KFREES(buf, len);
		return 0;
	}
	iplused[dev] += len;
	MUTEX_EXIT(&ipl_mutex);

	/*
	 * advance the log pointer to the next empty record and deduct the
	 * amount of space we're going to use.
	 */
	ipl = (iplog_t *)buf;
	ipl->ipl_magic = IPL_MAGIC;
	ipl->ipl_count = 1;
	ipl->ipl_next = NULL;
	ipl->ipl_dsize = len;
# ifdef _KERNEL
#  if SOLARIS || defined(sun)
	uniqtime(&ipl->ipl_tv);
#  else
#   if BSD >= 199306 || defined(__FreeBSD__) || defined(__sgi)
	microtime(&ipl->ipl_tv);
#   endif
#  endif
# else
	ipl->ipl_sec = 0;
	ipl->ipl_usec = 0;
# endif

	/*
	 * Loop through all the items to be logged, copying each one to the
	 * buffer.  Use bcopy for normal data or the mb_t copyout routine.
	 */
	for (i = 0, s = buf + IPLOG_SIZE; i < cnt; i++) {
		if (types[i] == 0)
			bcopy(items[i], s, itemsz[i]);
		else if (types[i] == 1) {
# if SOLARIS && defined(_KERNEL)
			copyout_mblk(items[i], 0, itemsz[i], s);
# else
			m_copydata(items[i], 0, itemsz[i], s);
# endif
		}
		s += itemsz[i];
	}
	MUTEX_ENTER(&ipl_mutex);
	ipll[dev] = ipl;
	*iplh[dev] = ipl;
	iplh[dev] = &ipl->ipl_next;
# if SOLARIS && defined(_KERNEL)
	cv_signal(&iplwait);
	mutex_exit(&ipl_mutex);
# else
	MUTEX_EXIT(&ipl_mutex);
	WAKEUP(&iplh[dev]);
# endif
	return 1;
}


int ipflog_read(unit, uio)
minor_t unit;
struct uio *uio;
{
	size_t dlen, copied;
	int error = 0;
	iplog_t *ipl;
# if defined(_KERNEL) && !SOLARIS
	int s;
# endif

	/*
	 * Sanity checks.  Make sure the minor # is valid and we're copying
	 * a valid chunk of data.
	 */
	if (IPL_LOGMAX < unit)
		return ENXIO;
	if (!uio->uio_resid)
		return 0;
	if (uio->uio_resid < IPLOG_SIZE)
		return EINVAL;
 
	/*
	 * Lock the log so we can snapshot the variables.  Wait for a signal
	 * if the log is empty.
	 */
	SPL_NET(s);
	MUTEX_ENTER(&ipl_mutex);

	while (!iplused[unit] || !iplt[unit]) {
# if SOLARIS && defined(_KERNEL)
		if (!cv_wait_sig(&iplwait, &ipl_mutex)) {
			MUTEX_EXIT(&ipl_mutex);
			return EINTR;
		}
# else
		MUTEX_EXIT(&ipl_mutex);
		error = SLEEP(&iplh[unit], "ipl sleep");
		if (error) {
			SPL_X(s);
			return error;
		}
		MUTEX_ENTER(&ipl_mutex);
# endif /* SOLARIS */
	}

# if BSD >= 199306 || defined(__FreeBSD__)
	uio->uio_rw = UIO_READ;
# endif

	for (copied = 0; (ipl = iplt[unit]); copied += dlen) {
		dlen = ipl->ipl_dsize;
		if (dlen > uio->uio_resid)
			break;
		/*
		 * Don't hold the mutex over the uiomove call.
		 */
		iplt[unit] = ipl->ipl_next;
		iplused[unit] -= dlen;
		MUTEX_EXIT(&ipl_mutex);
		error = UIOMOVE((caddr_t)ipl, dlen, UIO_READ, uio);
		MUTEX_ENTER(&ipl_mutex);
		if (error) {
			ipl->ipl_next = iplt[unit];
			iplt[unit] = ipl;
			iplused[unit] += dlen;
			break;
		}
		KFREES((caddr_t)ipl, dlen);
	}
	if (!iplt[unit]) {
		iplused[unit] = 0;
		iplh[unit] = &iplt[unit];
		ipll[unit] = NULL;
	}

	MUTEX_EXIT(&ipl_mutex);
	SPL_X(s);
	return error;
}


int ipflog_clear(unit)
minor_t unit;
{
	iplog_t *ipl;
	int used;

	MUTEX_ENTER(&ipl_mutex);
	while ((ipl = iplt[unit])) {
		iplt[unit] = ipl->ipl_next;
		KFREES((caddr_t)ipl, ipl->ipl_dsize);
	}
	iplh[unit] = &iplt[unit];
	ipll[unit] = NULL;
	used = iplused[unit];
	iplused[unit] = 0;
	bzero((char *)&iplcrc[unit], FI_LCSIZE);
	MUTEX_EXIT(&ipl_mutex);
	return used;
}
#endif /* IPFILTER_LOG */
