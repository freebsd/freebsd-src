/*
 * Copyright (C) 1998-2000 by Darren Reed & Guido van Rooij.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#if !defined(lint)
/*static const char rcsid[] = "@(#)$Id: ip_auth.c,v 2.1.2.2 2000/01/16 10:12:14 darrenr Exp $";*/
static const char rcsid[] = "@(#)$FreeBSD$";
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
#include <sys/uio.h>
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
extern struct ifqueue   ipintrq;                /* ip packet input queue */
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



#if (SOLARIS || defined(__sgi)) && defined(_KERNEL)
extern KRWLOCK_T ipf_auth;
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
frentry_t	*ipauth = NULL;


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
	u_32_t pass;
	int i;

	if (fr_auth_lock)
		return 0;

	READ_ENTER(&ipf_auth);
	for (i = fr_authstart; i != fr_authend; ) {
		/*
		 * index becomes -2 only after an SIOCAUTHW.  Check this in
		 * case the same packet gets sent again and it hasn't yet been
		 * auth'd.
		 */
		if ((fr_auth[i].fra_index == -2) &&
		    (id == fr_auth[i].fra_info.fin_id) &&
		    !bcmp((char *)fin,(char *)&fr_auth[i].fra_info,FI_CSIZE)) {
			/*
			 * Avoid feedback loop.
			 */
			if (!(pass = fr_auth[i].fra_pass) || (pass & FR_AUTH))
				pass = FR_BLOCK;
			RWLOCK_EXIT(&ipf_auth);
			WRITE_ENTER(&ipf_auth);
			fr_authstats.fas_hits++;
			fr_auth[i].fra_index = -1;
			fr_authused--;
			if (i == fr_authstart) {
				while (fr_auth[i].fra_index == -1) {
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
	int i;

	if (fr_auth_lock)
		return 0;

	WRITE_ENTER(&ipf_auth);
	if (fr_authstart > fr_authend) {
		fr_authstats.fas_nospace++;
		RWLOCK_EXIT(&ipf_auth);
		return 0;
	} else {
		if ((fr_authstart == 0) && (fr_authend == FR_NUMAUTH - 1)) {
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
	fr_auth[i].fra_index = i;
	fr_auth[i].fra_pass = 0;
	fr_auth[i].fra_age = fr_defaultauthage;
	bcopy((char *)fin, (char *)&fr_auth[i].fra_info, sizeof(*fin));
#if !defined(sparc) && !defined(m68k)
	/*
	 * No need to copyback here as we want to undo the changes, not keep
	 * them.
	 */
# if SOLARIS && defined(_KERNEL)
	if ((ip == (ip_t *)m->b_rptr) && (ip->ip_v == 4))
# endif
	{
		register u_short bo;

		bo = ip->ip_len;
		ip->ip_len = htons(bo);
# if !SOLARIS && !defined(__NetBSD__) && !defined(__FreeBSD__)
		/* 4.4BSD converts this ip_input.c, but I don't in solaris.c */
		bo = ip->ip_id;
		ip->ip_id = htons(bo);
# endif
		bo = ip->ip_off;
		ip->ip_off = htons(bo);
	}
#endif
#if SOLARIS && defined(_KERNEL)
	m->b_rptr -= qif->qf_off;
	fr_authpkts[i] = *(mblk_t **)fin->fin_mp;
	fr_auth[i].fra_q = qif->qf_q;
	cv_signal(&ipfauthwait);
#else
	fr_authpkts[i] = m;
# if defined(linux) && defined(_KERNEL)
	wake_up_interruptible(&ipfauthwait);
# else
	WAKEUP(&fr_authnext);
# endif
#endif
	return 1;
}


int fr_auth_ioctl(data, cmd, fr, frptr)
caddr_t data;
#if defined(__NetBSD__) || defined(__OpenBSD__) || (FreeBSD_version >= 300003)
u_long cmd;
#else
int cmd;
#endif
frentry_t *fr, **frptr;
{
	mb_t *m;
#if defined(_KERNEL) && !SOLARIS
	struct ifqueue *ifq;
#endif
	frauth_t auth, *au = &auth;
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
			if (!fae)
				error = ESRCH;
			else {
				WRITE_ENTER(&ipf_auth);
				*faep = fae->fae_next;
				*frptr = fr->fr_next;
				RWLOCK_EXIT(&ipf_auth);
				KFREE(fae);
			}
		} else {
			KMALLOC(fae, frauthent_t *);
			if (fae != NULL) {
				bcopy((char *)fr, (char *)&fae->fae_fr,
				      sizeof(*fr));
				WRITE_ENTER(&ipf_auth);
				fae->fae_age = fr_defaultauthage;
				fae->fae_fr.fr_hits = 0;
				fae->fae_fr.fr_next = *frptr;
				*frptr = &fae->fae_fr;
				fae->fae_next = *faep;
				*faep = fae;
				ipauth = &fae_list->fae_fr;
				RWLOCK_EXIT(&ipf_auth);
			} else
				error = ENOMEM;
		}
		break;
	case SIOCATHST:
		READ_ENTER(&ipf_auth);
		fr_authstats.fas_faelist = fae_list;
		RWLOCK_EXIT(&ipf_auth);
		error = IWCOPYPTR((char *)&fr_authstats, data,
				   sizeof(fr_authstats));
		break;
	case SIOCAUTHW:
fr_authioctlloop:
		READ_ENTER(&ipf_auth);
		if ((fr_authnext != fr_authend) && fr_authpkts[fr_authnext]) {
			error = IWCOPYPTR((char *)&fr_auth[fr_authnext], data,
					  sizeof(fr_info_t));
			RWLOCK_EXIT(&ipf_auth);
			if (error)
				break;
			WRITE_ENTER(&ipf_auth);
			fr_authnext++;
			if (fr_authnext == FR_NUMAUTH)
				fr_authnext = 0;
			RWLOCK_EXIT(&ipf_auth);
			return 0;
		}
#ifdef	_KERNEL
# if	SOLARIS
		mutex_enter(&ipf_authmx);
		if (!cv_wait_sig(&ipfauthwait, &ipf_authmx)) {
			mutex_exit(&ipf_authmx);
			return EINTR;
		}
		mutex_exit(&ipf_authmx);
# else
#  ifdef linux
		interruptible_sleep_on(&ipfauthwait);
		if (current->signal & ~current->blocked)
			error = -EINTR;
#  else
		error = SLEEP(&fr_authnext, "fr_authnext");
# endif
# endif
#endif
		RWLOCK_EXIT(&ipf_auth);
		if (!error)
			goto fr_authioctlloop;
		break;
	case SIOCAUTHR:
		error = IRCOPYPTR(data, (caddr_t)&auth, sizeof(auth));
		if (error)
			return error;
		WRITE_ENTER(&ipf_auth);
		i = au->fra_index;
		if ((i < 0) || (i > FR_NUMAUTH) ||
		    (fr_auth[i].fra_info.fin_id != au->fra_info.fin_id)) {
			RWLOCK_EXIT(&ipf_auth);
			return EINVAL;
		}
		m = fr_authpkts[i];
		fr_auth[i].fra_index = -2;
		fr_auth[i].fra_pass = au->fra_pass;
		fr_authpkts[i] = NULL;
#ifdef	_KERNEL
		RWLOCK_EXIT(&ipf_auth);
# ifndef linux
		if (m && au->fra_info.fin_out) {
#  if SOLARIS
			error = fr_qout(fr_auth[i].fra_q, m);
#  else /* SOLARIS */
#   if (_BSDI_VERSION >= 199802) || defined(__OpenBSD__)
			error = ip_output(m, NULL, NULL, IP_FORWARDING, NULL,
					  NULL);
#   else
			error = ip_output(m, NULL, NULL, IP_FORWARDING, NULL);
#   endif
#  endif /* SOLARIS */
			if (error)
				fr_authstats.fas_sendfail++;
			else
				fr_authstats.fas_sendok++;
		} else if (m) {
# if SOLARIS
			error = fr_qin(fr_auth[i].fra_q, m);
# else /* SOLARIS */
			ifq = &ipintrq;
			if (IF_QFULL(ifq)) {
				IF_DROP(ifq);
				m_freem(m);
				error = ENOBUFS;
			} else {
				IF_ENQUEUE(ifq, m);
				schednetisr(NETISR_IP);
			}
# endif /* SOLARIS */
			if (error)
				fr_authstats.fas_quefail++;
			else
				fr_authstats.fas_queok++;
		} else
			error = EINVAL;
# endif
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
			fr_auth[i].fra_index = -1;
			fr_auth[i].fra_pass = 0;
			if (i == fr_authstart) {
				while (fr_auth[i].fra_index == -1) {
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
		break;
	default :
		error = EINVAL;
		break;
	}
	return error;
}


#ifdef	_KERNEL
/*
 * Free all network buffer memory used to keep saved packets.
 */
void fr_authunload()
{
	register int i;
	register frauthent_t *fae, **faep;
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
	mb_t *m;
#if !SOLARIS
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
	ipauth = &fae_list->fae_fr;
	RWLOCK_EXIT(&ipf_auth);
	SPL_X(s);
}
#endif
