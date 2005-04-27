/*	$FreeBSD$	*/

/*
 * Copyright (C) 1998-2003 by Darren Reed & Guido van Rooij.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if defined(KERNEL) || defined(_KERNEL)
# undef KERNEL
# undef _KERNEL
# define        KERNEL	1
# define        _KERNEL	1
#endif
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#if !defined(_KERNEL)
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# define _KERNEL
# ifdef __OpenBSD__
struct file;
# endif
# include <sys/uio.h>
# undef _KERNEL
#endif
#if defined(_KERNEL) && (__FreeBSD_version >= 220000)
# include <sys/filio.h>
# include <sys/fcntl.h>
#else
# include <sys/ioctl.h>
#endif
#if !defined(linux)
# include <sys/protosw.h>
#endif
#include <sys/socket.h>
#if defined(_KERNEL)
# include <sys/systm.h>
# if !defined(__SVR4) && !defined(__svr4__) && !defined(linux)
#  include <sys/mbuf.h>
# endif
#endif
#if defined(__SVR4) || defined(__svr4__)
# include <sys/filio.h>
# include <sys/byteorder.h>
# ifdef _KERNEL
#  include <sys/dditypes.h>
# endif
# include <sys/stream.h>
# include <sys/kmem.h>
#endif
#if (_BSDI_VERSION >= 199802) || (__FreeBSD_version >= 400000)
# include <sys/queue.h>
#endif
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(bsdi)
# include <machine/cpu.h>
#endif
#if defined(_KERNEL) && defined(__NetBSD__) && (__NetBSD_Version__ >= 104000000)
# include <sys/proc.h>
#endif
#include <net/if.h>
#ifdef sun
# include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#if !defined(_KERNEL) && !defined(__osf__) && !defined(__sgi)
# define	KERNEL
# define	_KERNEL
# define	NOT_KERNEL
#endif
#if !defined(linux)
# include <netinet/ip_var.h>
#endif
#ifdef	NOT_KERNEL
# undef	_KERNEL
# undef	KERNEL
#endif
#include <netinet/tcp.h>
#if defined(IRIX) && (IRIX < 60516) /* IRIX < 6 */
extern struct ifqueue   ipintrq;		/* ip packet input queue */
#else
# if !defined(__hpux) && !defined(linux)
#  if __FreeBSD_version >= 300000
#   include <net/if_var.h>
#   if __FreeBSD_version >= 500042
#    define IF_QFULL _IF_QFULL
#    define IF_DROP _IF_DROP
#   endif /* __FreeBSD_version >= 500042 */
#  endif
#  include <netinet/in_var.h>
#  include <netinet/tcp_fsm.h>
# endif
#endif
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#include <netinet/tcpip.h>
#include "netinet/ip_fil.h"
#include "netinet/ip_auth.h"
#if !defined(MENTAT) && !defined(linux)
# include <net/netisr.h>
# ifdef __FreeBSD__
#  include <machine/cpufunc.h>
# endif
#endif
#if (__FreeBSD_version >= 300000)
# include <sys/malloc.h>
# if defined(_KERNEL) && !defined(IPFILTER_LKM)
#  include <sys/libkern.h>
#  include <sys/systm.h>
# endif
#endif
/* END OF INCLUDES */

#if !defined(lint)
static const char rcsid[] = "@(#)$FreeBSD$";
/* static const char rcsid[] = "@(#)Id: ip_auth.c,v 2.73.2.3 2004/08/26 11:25:21 darrenr Exp"; */
#endif


#if SOLARIS
extern kcondvar_t ipfauthwait;
#endif /* SOLARIS */
#if defined(linux) && defined(_KERNEL)
wait_queue_head_t     fr_authnext_linux;
#endif

int	fr_authsize = FR_NUMAUTH;
int	fr_authused = 0;
int	fr_defaultauthage = 600;
int	fr_auth_lock = 0;
int	fr_auth_init = 0;
fr_authstat_t	fr_authstats;
static frauth_t *fr_auth = NULL;
mb_t	**fr_authpkts = NULL;
int	fr_authstart = 0, fr_authend = 0, fr_authnext = 0;
frauthent_t	*fae_list = NULL;
frentry_t	*ipauth = NULL,
		*fr_authlist = NULL;


int fr_authinit()
{
	KMALLOCS(fr_auth, frauth_t *, fr_authsize * sizeof(*fr_auth));
	if (fr_auth != NULL)
		bzero((char *)fr_auth, fr_authsize * sizeof(*fr_auth));
	else
		return -1;

	KMALLOCS(fr_authpkts, mb_t **, fr_authsize * sizeof(*fr_authpkts));
	if (fr_authpkts != NULL)
		bzero((char *)fr_authpkts, fr_authsize * sizeof(*fr_authpkts));
	else
		return -2;

	MUTEX_INIT(&ipf_authmx, "ipf auth log mutex");
	RWLOCK_INIT(&ipf_auth, "ipf IP User-Auth rwlock");
#if SOLARIS && defined(_KERNEL)
	cv_init(&ipfauthwait, "ipf auth condvar", CV_DRIVER, NULL);
#endif
#if defined(linux) && defined(_KERNEL)
	init_waitqueue_head(&fr_authnext_linux);
#endif

	fr_auth_init = 1;

	return 0;
}


/*
 * Check if a packet has authorization.  If the packet is found to match an
 * authorization result and that would result in a feedback loop (i.e. it
 * will end up returning FR_AUTH) then return FR_BLOCK instead.
 */
frentry_t *fr_checkauth(fin, passp)
fr_info_t *fin;
u_32_t *passp;
{
	frentry_t *fr;
	frauth_t *fra;
	u_32_t pass;
	u_short id;
	ip_t *ip;
	int i;

	if (fr_auth_lock || !fr_authused)
		return NULL;

	ip = fin->fin_ip;
	id = ip->ip_id;

	READ_ENTER(&ipf_auth);
	for (i = fr_authstart; i != fr_authend; ) {
		/*
		 * index becomes -2 only after an SIOCAUTHW.  Check this in
		 * case the same packet gets sent again and it hasn't yet been
		 * auth'd.
		 */
		fra = fr_auth + i;
		if ((fra->fra_index == -2) && (id == fra->fra_info.fin_id) &&
		    !bcmp((char *)fin, (char *)&fra->fra_info, FI_CSIZE)) {
			/*
			 * Avoid feedback loop.
			 */
			if (!(pass = fra->fra_pass) || (FR_ISAUTH(pass)))
				pass = FR_BLOCK;
			/*
			 * Create a dummy rule for the stateful checking to
			 * use and return.  Zero out any values we don't
			 * trust from userland!
			 */
			if ((pass & FR_KEEPSTATE) || ((pass & FR_KEEPFRAG) &&
			     (fin->fin_flx & FI_FRAG))) {
				KMALLOC(fr, frentry_t *);
				if (fr) {
					bcopy((char *)fra->fra_info.fin_fr,
					      (char *)fr, sizeof(*fr));
					fr->fr_grp = NULL;
					fr->fr_ifa = fin->fin_ifp;
					fr->fr_func = NULL;
					fr->fr_ref = 1;
					fr->fr_flags = pass;
					fr->fr_ifas[1] = NULL;
					fr->fr_ifas[2] = NULL;
					fr->fr_ifas[3] = NULL;
				}
			} else
				fr = fra->fra_info.fin_fr;
			fin->fin_fr = fr;
			RWLOCK_EXIT(&ipf_auth);
			WRITE_ENTER(&ipf_auth);
			if ((fr != NULL) && (fr != fra->fra_info.fin_fr)) {
				fr->fr_next = fr_authlist;
				fr_authlist = fr;
			}
			fr_authstats.fas_hits++;
			fra->fra_index = -1;
			fr_authused--;
			if (i == fr_authstart) {
				while (fra->fra_index == -1) {
					i++;
					fra++;
					if (i == fr_authsize) {
						i = 0;
						fra = fr_auth;
					}
					fr_authstart = i;
					if (i == fr_authend)
						break;
				}
				if (fr_authstart == fr_authend) {
					fr_authnext = 0;
					fr_authstart = fr_authend = 0;
				}
			}
			RWLOCK_EXIT(&ipf_auth);
			if (passp != NULL)
				*passp = pass;
			ATOMIC_INC64(fr_authstats.fas_hits);
			return fr;
		}
		i++;
		if (i == fr_authsize)
			i = 0;
	}
	fr_authstats.fas_miss++;
	RWLOCK_EXIT(&ipf_auth);
	ATOMIC_INC64(fr_authstats.fas_miss);
	return NULL;
}


/*
 * Check if we have room in the auth array to hold details for another packet.
 * If we do, store it and wake up any user programs which are waiting to
 * hear about these events.
 */
int fr_newauth(m, fin)
mb_t *m;
fr_info_t *fin;
{
#if defined(_KERNEL) && defined(MENTAT)
	qpktinfo_t *qpi = fin->fin_qpi;
#endif
	frauth_t *fra;
#if !defined(sparc) && !defined(m68k)
	ip_t *ip;
#endif
	int i;

	if (fr_auth_lock)
		return 0;

	WRITE_ENTER(&ipf_auth);
	if (fr_authstart > fr_authend) {
		fr_authstats.fas_nospace++;
		RWLOCK_EXIT(&ipf_auth);
		return 0;
	} else {
		if (fr_authused == fr_authsize) {
			fr_authstats.fas_nospace++;
			RWLOCK_EXIT(&ipf_auth);
			return 0;
		}
	}

	fr_authstats.fas_added++;
	fr_authused++;
	i = fr_authend++;
	if (fr_authend == fr_authsize)
		fr_authend = 0;
	RWLOCK_EXIT(&ipf_auth);

	fra = fr_auth + i;
	fra->fra_index = i;
	fra->fra_pass = 0;
	fra->fra_age = fr_defaultauthage;
	bcopy((char *)fin, (char *)&fra->fra_info, sizeof(*fin));
#if !defined(sparc) && !defined(m68k)
	/*
	 * No need to copyback here as we want to undo the changes, not keep
	 * them.
	 */
	ip = fin->fin_ip;
# if defined(MENTAT) && defined(_KERNEL)
	if ((ip == (ip_t *)m->b_rptr) && (fin->fin_v == 4))
# endif
	{
		register u_short bo;

		bo = ip->ip_len;
		ip->ip_len = htons(bo);
		bo = ip->ip_off;
		ip->ip_off = htons(bo);
	}
#endif
#if SOLARIS && defined(_KERNEL)
	m->b_rptr -= qpi->qpi_off;
	fr_authpkts[i] = *(mblk_t **)fin->fin_mp;
	fra->fra_q = qpi->qpi_q;	/* The queue can disappear! */
	cv_signal(&ipfauthwait);
#else
# if defined(BSD) && !defined(sparc) && (BSD >= 199306)
	if (!fin->fin_out) {
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
	}
# endif
	fr_authpkts[i] = m;
	WAKEUP(&fr_authnext,0);
#endif
	return 1;
}


int fr_auth_ioctl(data, cmd, mode)
caddr_t data;
ioctlcmd_t cmd;
int mode;
{
	mb_t *m;
#if defined(_KERNEL) && !defined(MENTAT) && !defined(linux) && \
    (!defined(__FreeBSD_version) || (__FreeBSD_version < 501000))
	struct ifqueue *ifq;
# ifdef USE_SPL
	int s;
# endif /* USE_SPL */
#endif
	frauth_t auth, *au = &auth, *fra;
	int i, error = 0, len;
	char *t;

	switch (cmd)
	{
	case SIOCSTLCK :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		fr_lock(data, &fr_auth_lock);
		break;

	case SIOCATHST:
		fr_authstats.fas_faelist = fae_list;
		error = fr_outobj(data, &fr_authstats, IPFOBJ_AUTHSTAT);
		break;

	case SIOCIPFFL:
		SPL_NET(s);
		WRITE_ENTER(&ipf_auth);
		i = fr_authflush();
		RWLOCK_EXIT(&ipf_auth);
		SPL_X(s);
		error = copyoutptr((char *)&i, data, sizeof(i));
		break;

	case SIOCAUTHW:
fr_authioctlloop:
		error = fr_inobj(data, au, IPFOBJ_FRAUTH);
		READ_ENTER(&ipf_auth);
		if ((fr_authnext != fr_authend) && fr_authpkts[fr_authnext]) {
			error = fr_outobj(data, &fr_auth[fr_authnext],
					  IPFOBJ_FRAUTH);
			if (auth.fra_len != 0 && auth.fra_buf != NULL) {
				/*
				 * Copy packet contents out to user space if
				 * requested.  Bail on an error.
				 */
				m = fr_authpkts[fr_authnext];
				len = MSGDSIZE(m);
				if (len > auth.fra_len)
					len = auth.fra_len;
				auth.fra_len = len;
				for (t = auth.fra_buf; m && (len > 0); ) {
					i = MIN(M_LEN(m), len);
					error = copyoutptr(MTOD(m, char *),
							  t, i);
					len -= i;
					t += i;
					if (error != 0)
						break;
				}
			}
			RWLOCK_EXIT(&ipf_auth);
			if (error != 0)
				break;
			SPL_NET(s);
			WRITE_ENTER(&ipf_auth);
			fr_authnext++;
			if (fr_authnext == fr_authsize)
				fr_authnext = 0;
			RWLOCK_EXIT(&ipf_auth);
			SPL_X(s);
			return 0;
		}
		RWLOCK_EXIT(&ipf_auth);
		/*
		 * We exit ipf_global here because a program that enters in
		 * here will have a lock on it and goto sleep having this lock.
		 * If someone were to do an 'ipf -D' the system would then
		 * deadlock.  The catch with releasing it here is that the
		 * caller of this function expects it to be held when we
		 * return so we have to reacquire it in here.
		 */
		RWLOCK_EXIT(&ipf_global);

		MUTEX_ENTER(&ipf_authmx);
#ifdef	_KERNEL
# if	SOLARIS
		error = 0;
		if (!cv_wait_sig(&ipfauthwait, &ipf_authmx.ipf_lk))
			error = EINTR;
# else /* SOLARIS */
#  ifdef __hpux
		{
		lock_t *l;

		l = get_sleep_lock(&fr_authnext);
		error = sleep(&fr_authnext, PZERO+1);
		spinunlock(l);
		}
#  else
#   ifdef __osf__
		error = mpsleep(&fr_authnext, PSUSP|PCATCH, "fr_authnext", 0,
				&ipf_authmx, MS_LOCK_SIMPLE);
#   else
		error = SLEEP(&fr_authnext, "fr_authnext");
#   endif /* __osf__ */
#  endif /* __hpux */
# endif /* SOLARIS */
#endif
		MUTEX_EXIT(&ipf_authmx);
		READ_ENTER(&ipf_global);
		if (error == 0) {
			READ_ENTER(&ipf_auth);
			goto fr_authioctlloop;
		}
		break;

	case SIOCAUTHR:
		error = fr_inobj(data, &auth, IPFOBJ_FRAUTH);
		if (error != 0)
			return error;
		SPL_NET(s);
		WRITE_ENTER(&ipf_auth);
		i = au->fra_index;
		fra = fr_auth + i;
		if ((i < 0) || (i >= fr_authsize) ||
		    (fra->fra_info.fin_id != au->fra_info.fin_id)) {
			RWLOCK_EXIT(&ipf_auth);
			SPL_X(s);
			return ESRCH;
		}
		m = fr_authpkts[i];
		fra->fra_index = -2;
		fra->fra_pass = au->fra_pass;
		fr_authpkts[i] = NULL;
		RWLOCK_EXIT(&ipf_auth);
#ifdef	_KERNEL
		if ((m != NULL) && (au->fra_info.fin_out != 0)) {
# ifdef MENTAT
			error = !putq(fra->fra_q, m);
# else /* MENTAT */
#  ifdef linux
#  else
#   if (_BSDI_VERSION >= 199802) || defined(__OpenBSD__) || \
       (defined(__sgi) && (IRIX >= 60500) || \
       (defined(__FreeBSD__) && (__FreeBSD_version >= 470102)))
			error = ip_output(m, NULL, NULL, IP_FORWARDING, NULL,
					  NULL);
#   else
			error = ip_output(m, NULL, NULL, IP_FORWARDING, NULL);
#   endif
#  endif /* Linux */
# endif /* MENTAT */
			if (error != 0)
				fr_authstats.fas_sendfail++;
			else
				fr_authstats.fas_sendok++;
		} else if (m) {
# ifdef MENTAT
			error = !putq(fra->fra_q, m);
# else /* MENTAT */
#  ifdef linux
#  else
#   if __FreeBSD_version >= 501000
			netisr_dispatch(NETISR_IP, m);
#   else
#    if IRIX >= 60516
			ifq = &((struct ifnet *)fra->fra_info.fin_ifp)->if_snd;
#    else
			ifq = &ipintrq;
#    endif
			if (IF_QFULL(ifq)) {
				IF_DROP(ifq);
				FREE_MB_T(m);
				error = ENOBUFS;
			} else {
				IF_ENQUEUE(ifq, m);
#    if IRIX < 60500
				schednetisr(NETISR_IP);
#    endif
			}
#   endif
#  endif /* Linux */
# endif /* MENTAT */
			if (error != 0)
				fr_authstats.fas_quefail++;
			else
				fr_authstats.fas_queok++;
		} else
			error = EINVAL;
# ifdef MENTAT
		if (error != 0)
			error = EINVAL;
# else /* MENTAT */
		/*
		 * If we experience an error which will result in the packet
		 * not being processed, make sure we advance to the next one.
		 */
		if (error == ENOBUFS) {
			fr_authused--;
			fra->fra_index = -1;
			fra->fra_pass = 0;
			if (i == fr_authstart) {
				while (fra->fra_index == -1) {
					i++;
					if (i == fr_authsize)
						i = 0;
					fr_authstart = i;
					if (i == fr_authend)
						break;
				}
				if (fr_authstart == fr_authend) {
					fr_authnext = 0;
					fr_authstart = fr_authend = 0;
				}
			}
		}
# endif /* MENTAT */
#endif /* _KERNEL */
		SPL_X(s);
		break;

	default :
		error = EINVAL;
		break;
	}
	return error;
}


/*
 * Free all network buffer memory used to keep saved packets.
 */
void fr_authunload()
{
	register int i;
	register frauthent_t *fae, **faep;
	frentry_t *fr, **frp;
	mb_t *m;

	if (fr_auth != NULL) {
		KFREES(fr_auth, fr_authsize * sizeof(*fr_auth));
		fr_auth = NULL;
	}

	if (fr_authpkts != NULL) {
		for (i = 0; i < fr_authsize; i++) {
			m = fr_authpkts[i];
			if (m != NULL) {
				FREE_MB_T(m);
				fr_authpkts[i] = NULL;
			}
		}
		KFREES(fr_authpkts, fr_authsize * sizeof(*fr_authpkts));
		fr_authpkts = NULL;
	}

	faep = &fae_list;
	while ((fae = *faep) != NULL) {
		*faep = fae->fae_next;
		KFREE(fae);
	}
	ipauth = NULL;

	if (fr_authlist != NULL) {
		for (frp = &fr_authlist; ((fr = *frp) != NULL); ) {
			if (fr->fr_ref == 1) {
				*frp = fr->fr_next;
				KFREE(fr);
			} else
				frp = &fr->fr_next;
		}
	}

	if (fr_auth_init == 1) {
# if SOLARIS && defined(_KERNEL)
		cv_destroy(&ipfauthwait);
# endif
		MUTEX_DESTROY(&ipf_authmx);
		RW_DESTROY(&ipf_auth);

		fr_auth_init = 0;
	}
}


/*
 * Slowly expire held auth records.  Timeouts are set
 * in expectation of this being called twice per second.
 */
void fr_authexpire()
{
	register int i;
	register frauth_t *fra;
	register frauthent_t *fae, **faep;
	register frentry_t *fr, **frp;
	mb_t *m;
# if !defined(MENAT) && defined(_KERNEL) && defined(USE_SPL)
	int s;
# endif

	if (fr_auth_lock)
		return;

	SPL_NET(s);
	WRITE_ENTER(&ipf_auth);
	for (i = 0, fra = fr_auth; i < fr_authsize; i++, fra++) {
		fra->fra_age--;
		if ((fra->fra_age == 0) && (m = fr_authpkts[i])) {
			FREE_MB_T(m);
			fr_authpkts[i] = NULL;
			fr_auth[i].fra_index = -1;
			fr_authstats.fas_expire++;
			fr_authused--;
		}
	}

	for (faep = &fae_list; ((fae = *faep) != NULL); ) {
		fae->fae_age--;
		if (fae->fae_age == 0) {
			*faep = fae->fae_next;
			KFREE(fae);
			fr_authstats.fas_expire++;
		} else
			faep = &fae->fae_next;
	}
	if (fae_list != NULL)
		ipauth = &fae_list->fae_fr;
	else
		ipauth = NULL;

	for (frp = &fr_authlist; ((fr = *frp) != NULL); ) {
		if (fr->fr_ref == 1) {
			*frp = fr->fr_next;
			KFREE(fr);
		} else
			frp = &fr->fr_next;
	}
	RWLOCK_EXIT(&ipf_auth);
	SPL_X(s);
}

int fr_preauthcmd(cmd, fr, frptr)
ioctlcmd_t cmd;
frentry_t *fr, **frptr;
{
	frauthent_t *fae, **faep;
	int error = 0;
# if !defined(MENAT) && defined(_KERNEL) && defined(USE_SPL)
	int s;
#endif

	if ((cmd != SIOCADAFR) && (cmd != SIOCRMAFR))
		return EIO;
	
	for (faep = &fae_list; ((fae = *faep) != NULL); ) {
		if (&fae->fae_fr == fr)
			break;
		else
			faep = &fae->fae_next;
	}

	if (cmd == (ioctlcmd_t)SIOCRMAFR) {
		if (fr == NULL || frptr == NULL)
			error = EINVAL;
		else if (fae == NULL)
			error = ESRCH;
		else {
			SPL_NET(s);
			WRITE_ENTER(&ipf_auth);
			*faep = fae->fae_next;
			if (ipauth == &fae->fae_fr)
				ipauth = fae_list ? &fae_list->fae_fr : NULL;
			RWLOCK_EXIT(&ipf_auth);
			SPL_X(s);

			KFREE(fae);
		}
	} else if (fr != NULL && frptr != NULL) {
		KMALLOC(fae, frauthent_t *);
		if (fae != NULL) {
			bcopy((char *)fr, (char *)&fae->fae_fr,
			      sizeof(*fr));
			SPL_NET(s);
			WRITE_ENTER(&ipf_auth);
			fae->fae_age = fr_defaultauthage;
			fae->fae_fr.fr_hits = 0;
			fae->fae_fr.fr_next = *frptr;
			*frptr = &fae->fae_fr;
			fae->fae_next = *faep;
			*faep = fae;
			ipauth = &fae_list->fae_fr;
			RWLOCK_EXIT(&ipf_auth);
			SPL_X(s);
		} else
			error = ENOMEM;
	} else
		error = EINVAL;
	return error;
}


/*
 * Flush held packets.
 * Must already be properly SPL'ed and Locked on &ipf_auth.
 *
 */
int fr_authflush()
{
	register int i, num_flushed;
	mb_t *m;

	if (fr_auth_lock)
		return -1;

	num_flushed = 0;

	for (i = 0 ; i < fr_authsize; i++) {
		m = fr_authpkts[i];
		if (m != NULL) {
			FREE_MB_T(m);
			fr_authpkts[i] = NULL;
			fr_auth[i].fra_index = -1;
			/* perhaps add & use a flush counter inst.*/
			fr_authstats.fas_expire++;
			fr_authused--;
			num_flushed++;
		}
	}

	fr_authstart = 0;
	fr_authend = 0;
	fr_authnext = 0;

	return num_flushed;
}
