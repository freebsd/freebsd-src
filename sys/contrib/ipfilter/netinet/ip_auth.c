/*
 * Copyright (C) 1998-2001 by Darren Reed & Guido van Rooij.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#ifdef __sgi
# include <sys/ptimers.h>
#endif
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#if !defined(_KERNEL) && !defined(KERNEL)
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
#endif
#if (defined(KERNEL) || defined(_KERNEL)) && (__FreeBSD_version >= 220000)
# include <sys/filio.h>
# include <sys/fcntl.h>
#else
# include <sys/ioctl.h>
#endif
#ifndef linux
# include <sys/protosw.h>
#endif
#include <sys/socket.h>
#if (defined(_KERNEL) || defined(KERNEL)) && !defined(linux)
# include <sys/systm.h>
#endif
#if !defined(__SVR4) && !defined(__svr4__)
# ifndef linux
#  include <sys/mbuf.h>
# endif
#else
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
#include <net/if.h>
#ifdef sun
# include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifndef	KERNEL
# define	KERNEL
# define	NOT_KERNEL
#endif
#ifndef linux
# include <netinet/ip_var.h>
#endif
#ifdef	NOT_KERNEL
# undef	KERNEL
#endif
#ifdef __sgi
# ifdef IFF_DRVRLOCK /* IRIX6 */
#  include <sys/hashing.h>
# endif
#endif
#include <netinet/tcp.h>
#if defined(__sgi) && !defined(IFF_DRVRLOCK) /* IRIX < 6 */
extern struct ifqueue   ipintrq;		/* ip packet input queue */
#else
# ifndef linux
#  if __FreeBSD_version >= 300000
#   include <net/if_var.h>
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
#if !SOLARIS && !defined(linux)
# include <net/netisr.h>
# ifdef __FreeBSD__
#  include <machine/cpufunc.h>
# endif
#endif
#if (__FreeBSD_version >= 300000)
# include <sys/malloc.h>
# if (defined(_KERNEL) || defined(KERNEL)) && !defined(IPFILTER_LKM)
#  include <sys/libkern.h>
#  include <sys/systm.h>
# endif
#endif

#if !defined(lint)
/*static const char rcsid[] = "@(#)$Id: ip_auth.c,v 2.11.2.20 2002/06/04 14:40:42 darrenr Exp $";*/
static const char rcsid[] = "@(#)$FreeBSD$";
#endif


#if (SOLARIS || defined(__sgi)) && defined(_KERNEL)
extern KRWLOCK_T ipf_auth, ipf_mutex;
extern kmutex_t ipf_authmx;
# if SOLARIS
extern kcondvar_t ipfauthwait;
# endif
#endif
#ifdef linux
static struct wait_queue *ipfauthwait = NULL;
#endif

int	fr_authsize = FR_NUMAUTH;
int	fr_authused = 0;
int	fr_defaultauthage = 600;
int	fr_auth_lock = 0;
fr_authstat_t	fr_authstats;
static frauth_t fr_auth[FR_NUMAUTH];
mb_t	*fr_authpkts[FR_NUMAUTH];
static int	fr_authstart = 0, fr_authend = 0, fr_authnext = 0;
static frauthent_t	*fae_list = NULL;
frentry_t	*ipauth = NULL,
		*fr_authlist = NULL;


/*
 * Check if a packet has authorization.  If the packet is found to match an
 * authorization result and that would result in a feedback loop (i.e. it
 * will end up returning FR_AUTH) then return FR_BLOCK instead.
 */
u_32_t fr_checkauth(ip, fin)
ip_t *ip;
fr_info_t *fin;
{
	u_short id = ip->ip_id;
	frentry_t *fr;
	frauth_t *fra;
	u_32_t pass;
	int i;

	if (fr_auth_lock || !fr_authused)
		return 0;

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
			if (!(pass = fra->fra_pass) || (pass & FR_AUTH))
				pass = FR_BLOCK;
			/*
			 * Create a dummy rule for the stateful checking to
			 * use and return.  Zero out any values we don't
			 * trust from userland!
			 */
			if ((pass & FR_KEEPSTATE) || ((pass & FR_KEEPFRAG) &&
			     (fin->fin_fi.fi_fl & FI_FRAG))) {
				KMALLOC(fr, frentry_t *);
				if (fr) {
					bcopy((char *)fra->fra_info.fin_fr,
					      fr, sizeof(*fr));
					fr->fr_grp = NULL;
					fr->fr_ifa = fin->fin_ifp;
					fr->fr_func = NULL;
					fr->fr_ref = 1;
					fr->fr_flags = pass;
#if BSD >= 199306
					fr->fr_oifa = NULL;
#endif
				}
			} else
				fr = fra->fra_info.fin_fr;
			fin->fin_fr = fr;
			RWLOCK_EXIT(&ipf_auth);
			WRITE_ENTER(&ipf_auth);
			if (fr && fr != fra->fra_info.fin_fr) {
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
					if (i == FR_NUMAUTH) {
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
			return pass;
		}
		i++;
		if (i == FR_NUMAUTH)
			i = 0;
	}
	fr_authstats.fas_miss++;
	RWLOCK_EXIT(&ipf_auth);
	return 0;
}


/*
 * Check if we have room in the auth array to hold details for another packet.
 * If we do, store it and wake up any user programs which are waiting to
 * hear about these events.
 */
int fr_newauth(m, fin, ip)
mb_t *m;
fr_info_t *fin;
ip_t *ip;
{
#if defined(_KERNEL) && SOLARIS
	qif_t *qif = fin->fin_qif;
#endif
	frauth_t *fra;
	int i;

	if (fr_auth_lock)
		return 0;

	WRITE_ENTER(&ipf_auth);
	if (fr_authstart > fr_authend) {
		fr_authstats.fas_nospace++;
		RWLOCK_EXIT(&ipf_auth);
		return 0;
	} else {
		if (fr_authused == FR_NUMAUTH) {
			fr_authstats.fas_nospace++;
			RWLOCK_EXIT(&ipf_auth);
			return 0;
		}
	}

	fr_authstats.fas_added++;
	fr_authused++;
	i = fr_authend++;
	if (fr_authend == FR_NUMAUTH)
		fr_authend = 0;
	RWLOCK_EXIT(&ipf_auth);
	fra = fr_auth + i;
	fra->fra_index = i;
	fra->fra_pass = 0;
	fra->fra_age = fr_defaultauthage;
	bcopy((char *)fin, (char *)&fra->fra_info, sizeof(*fin));
#if SOLARIS && defined(_KERNEL)
# if !defined(sparc)
	/*
	 * No need to copyback here as we want to undo the changes, not keep
	 * them.
	 */
	if ((ip == (ip_t *)m->b_rptr) && (ip->ip_v == 4))
	{
		register u_short bo;

		bo = ip->ip_len;
		ip->ip_len = htons(bo);
		bo = ip->ip_off;
		ip->ip_off = htons(bo);
	}
# endif
	m->b_rptr -= qif->qf_off;
	fr_authpkts[i] = *(mblk_t **)fin->fin_mp;
	fra->fra_q = qif->qf_q;
	cv_signal(&ipfauthwait);
#else
# if defined(BSD) && !defined(sparc) && (BSD >= 199306)
	if (!fin->fin_out) {
		HTONS(ip->ip_len);
		HTONS(ip->ip_off);
	}
# endif
	fr_authpkts[i] = m;
	WAKEUP(&fr_authnext);
#endif
	return 1;
}


int fr_auth_ioctl(data, mode, cmd, fr, frptr)
caddr_t data;
int mode;
#if defined(__NetBSD__) || defined(__OpenBSD__) || (__FreeBSD_version >= 300003)
u_long cmd;
#else
int cmd;
#endif
frentry_t *fr, **frptr;
{
	mb_t *m;
#if defined(_KERNEL) && !SOLARIS
	struct ifqueue *ifq;
	int s;
#endif
	frauth_t auth, *au = &auth, *fra;
	frauthent_t *fae, **faep;
	int i, error = 0;

	switch (cmd)
	{
	case SIOCSTLCK :
		error = fr_lock(data, &fr_auth_lock);
		break;
	case SIOCINIFR :
	case SIOCRMIFR :
	case SIOCADIFR :
		error = EINVAL;
		break;
	case SIOCINAFR :
		error = EINVAL;
		break;
	case SIOCRMAFR :
	case SIOCADAFR :
		for (faep = &fae_list; (fae = *faep); )
			if (&fae->fae_fr == fr)
				break;
			else
				faep = &fae->fae_next;
		if (cmd == SIOCRMAFR) {
			if (!fr || !frptr)
				error = EINVAL;
			else if (!fae)
				error = ESRCH;
			else {
				WRITE_ENTER(&ipf_auth);
				SPL_NET(s);
				*faep = fae->fae_next;
				*frptr = fr->fr_next;
				SPL_X(s);
				RWLOCK_EXIT(&ipf_auth);
				KFREE(fae);
			}
		} else if (fr && frptr) {
			KMALLOC(fae, frauthent_t *);
			if (fae != NULL) {
				bcopy((char *)fr, (char *)&fae->fae_fr,
				      sizeof(*fr));
				WRITE_ENTER(&ipf_auth);
				SPL_NET(s);
				fae->fae_age = fr_defaultauthage;
				fae->fae_fr.fr_hits = 0;
				fae->fae_fr.fr_next = *frptr;
				*frptr = &fae->fae_fr;
				fae->fae_next = *faep;
				*faep = fae;
				ipauth = &fae_list->fae_fr;
				SPL_X(s);
				RWLOCK_EXIT(&ipf_auth);
			} else
				error = ENOMEM;
		} else
			error = EINVAL;
		break;
	case SIOCATHST:
		fr_authstats.fas_faelist = fae_list;
		error = IWCOPYPTR((char *)&fr_authstats, data,
				   sizeof(fr_authstats));
		break;
	case SIOCAUTHW:
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
fr_authioctlloop:
		READ_ENTER(&ipf_auth);
		if ((fr_authnext != fr_authend) && fr_authpkts[fr_authnext]) {
			error = IWCOPYPTR((char *)&fr_auth[fr_authnext], data,
					  sizeof(frauth_t));
			RWLOCK_EXIT(&ipf_auth);
			if (error)
				break;
			WRITE_ENTER(&ipf_auth);
			SPL_NET(s);
			fr_authnext++;
			if (fr_authnext == FR_NUMAUTH)
				fr_authnext = 0;
			SPL_X(s);
			RWLOCK_EXIT(&ipf_auth);
			return 0;
		}
		RWLOCK_EXIT(&ipf_auth);
#ifdef	_KERNEL
# if	SOLARIS
		mutex_enter(&ipf_authmx);
		if (!cv_wait_sig(&ipfauthwait, &ipf_authmx)) {
			mutex_exit(&ipf_authmx);
			return EINTR;
		}
		mutex_exit(&ipf_authmx);
# else
		error = SLEEP(&fr_authnext, "fr_authnext");
# endif
#endif
		if (!error)
			goto fr_authioctlloop;
		break;
	case SIOCAUTHR:
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		error = IRCOPYPTR(data, (caddr_t)&auth, sizeof(auth));
		if (error)
			return error;
		WRITE_ENTER(&ipf_auth);
		SPL_NET(s);
		i = au->fra_index;
		fra = fr_auth + i;
		if ((i < 0) || (i > FR_NUMAUTH) ||
		    (fra->fra_info.fin_id != au->fra_info.fin_id)) {
			SPL_X(s);
			RWLOCK_EXIT(&ipf_auth);
			return EINVAL;
		}
		m = fr_authpkts[i];
		fra->fra_index = -2;
		fra->fra_pass = au->fra_pass;
		fr_authpkts[i] = NULL;
		RWLOCK_EXIT(&ipf_auth);
#ifdef	_KERNEL
		if (m && au->fra_info.fin_out) {
# if SOLARIS
			error = (fr_qout(fra->fra_q, m) == 0) ? EINVAL : 0;
# else /* SOLARIS */
			struct route ro;

			bzero((char *)&ro, sizeof(ro));
#  if ((_BSDI_VERSION >= 199802) && (_BSDI_VERSION < 200005)) || \
       defined(__OpenBSD__) || (defined(IRIX) && (IRIX >= 605))
			error = ip_output(m, NULL, &ro, IP_FORWARDING, NULL,
					  NULL);
#  else
			error = ip_output(m, NULL, &ro, IP_FORWARDING, NULL);
#  endif
			if (ro.ro_rt) {
				RTFREE(ro.ro_rt);
			}
# endif /* SOLARIS */
			if (error)
				fr_authstats.fas_sendfail++;
			else
				fr_authstats.fas_sendok++;
		} else if (m) {
# if SOLARIS
			error = (fr_qin(fra->fra_q, m) == 0) ? EINVAL : 0;
# else /* SOLARIS */
			ifq = &ipintrq;
			if (IF_QFULL(ifq)) {
				IF_DROP(ifq);
				m_freem(m);
				error = ENOBUFS;
			} else {
				IF_ENQUEUE(ifq, m);
#  if IRIX < 605
				schednetisr(NETISR_IP);
#  endif
			}
# endif /* SOLARIS */
			if (error)
				fr_authstats.fas_quefail++;
			else
				fr_authstats.fas_queok++;
		} else
			error = EINVAL;
# if SOLARIS
		if (error)
			error = EINVAL;
# else
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
					if (i == FR_NUMAUTH)
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
# endif
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

	WRITE_ENTER(&ipf_auth);
	for (i = 0; i < FR_NUMAUTH; i++) {
		if ((m = fr_authpkts[i])) {
			FREE_MB_T(m);
			fr_authpkts[i] = NULL;
			fr_auth[i].fra_index = -1;
		}
	}


	for (faep = &fae_list; (fae = *faep); ) {
		*faep = fae->fae_next;
		KFREE(fae);
	}
	ipauth = NULL;
	RWLOCK_EXIT(&ipf_auth);

	if (fr_authlist) {
		/*
		 * We *MuST* reget ipf_auth because otherwise we won't get the
		 * locks in the right order and risk deadlock.
		 * We need ipf_mutex here to prevent a rule from using it
		 * inside fr_check().
		 */
		WRITE_ENTER(&ipf_mutex);
		WRITE_ENTER(&ipf_auth);
		for (frp = &fr_authlist; (fr = *frp); ) {
			if (fr->fr_ref == 1) {
				*frp = fr->fr_next;
				KFREE(fr);
			} else
				frp = &fr->fr_next;
		}
		RWLOCK_EXIT(&ipf_auth);
		RWLOCK_EXIT(&ipf_mutex);
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
#if !SOLARIS && defined(_KERNEL)
	int s;
#endif

	if (fr_auth_lock)
		return;

	SPL_NET(s);
	WRITE_ENTER(&ipf_auth);
	for (i = 0, fra = fr_auth; i < FR_NUMAUTH; i++, fra++) {
		if ((!--fra->fra_age) && (m = fr_authpkts[i])) {
			FREE_MB_T(m);
			fr_authpkts[i] = NULL;
			fr_auth[i].fra_index = -1;
			fr_authstats.fas_expire++;
			fr_authused--;
		}
	}

	for (faep = &fae_list; (fae = *faep); ) {
		if (!--fae->fae_age) {
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

	for (frp = &fr_authlist; (fr = *frp); ) {
		if (fr->fr_ref == 1) {
			*frp = fr->fr_next;
			KFREE(fr);
		} else
			frp = &fr->fr_next;
	}
	RWLOCK_EXIT(&ipf_auth);
	SPL_X(s);
}
