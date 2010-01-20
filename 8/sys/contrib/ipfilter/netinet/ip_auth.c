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
#if (defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802) || \
    (defined(__FreeBSD_version) &&(__FreeBSD_version >= 400000))
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
#if !defined(_KERNEL) && defined(__FreeBSD_version) && \
    __FreeBSD_version >= 800049
# define V_ip_do_randomid	ip_do_randomid
# define V_ip_id		ip_id
#endif
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
/* static const char rcsid[] = "@(#)$Id: ip_auth.c,v 2.73.2.24 2007/09/09 11:32:04 darrenr Exp $"; */
#endif


#if SOLARIS && defined(_KERNEL)
extern kcondvar_t ipfauthwait;
extern struct pollhead iplpollhead[IPL_LOGSIZE];
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

void fr_authderef __P((frauthent_t **));
int fr_authgeniter __P((ipftoken_t *, ipfgeniter_t *));
int fr_authreply __P((char *));
int fr_authwait __P((char *));

/* ------------------------------------------------------------------------ */
/* Function:    fr_authinit                                                 */
/* Returns:     int - 0 == success, else error                              */
/* Parameters:  None                                                        */
/*                                                                          */
/* Allocate memory and initialise data structures used in handling auth     */
/* rules.                                                                   */
/* ------------------------------------------------------------------------ */
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


/* ------------------------------------------------------------------------ */
/* Function:    fr_checkauth                                                */
/* Returns:     frentry_t* - pointer to ipf rule if match found, else NULL  */
/* Parameters:  fin(I)   - pointer to ipftoken structure                    */
/*              passp(I) - pointer to ipfgeniter structure                  */
/*                                                                          */
/* Check if a packet has authorization.  If the packet is found to match an */
/* authorization result and that would result in a feedback loop (i.e. it   */
/* will end up returning FR_AUTH) then return FR_BLOCK instead.             */
/* ------------------------------------------------------------------------ */
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
			/*
			 * fr_authlist is populated with the rules malloc'd
			 * above and only those.
			 */
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


/* ------------------------------------------------------------------------ */
/* Function:    fr_newauth                                                  */
/* Returns:     int - 1 == success, 0 = did not put packet on auth queue    */
/* Parameters:  m(I)   - pointer to mb_t with packet in it                  */
/*              fin(I) - pointer to packet information                      */
/*                                                                          */
/* Check if we have room in the auth array to hold details for another      */
/* packet. If we do, store it and wake up any user programs which are       */
/* waiting to hear about these events.                                      */
/* ------------------------------------------------------------------------ */
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
	if (((fr_authend + 1) % fr_authsize) == fr_authstart) {
		fr_authstats.fas_nospace++;
		RWLOCK_EXIT(&ipf_auth);
		return 0;
	}

	fr_authstats.fas_added++;
	fr_authused++;
	i = fr_authend++;
	if (fr_authend == fr_authsize)
		fr_authend = 0;
	fra = fr_auth + i;
	fra->fra_index = i;
	RWLOCK_EXIT(&ipf_auth);

	if (fin->fin_fr != NULL)
		fra->fra_pass = fin->fin_fr->fr_flags;
	else
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
	COPYIFNAME(fin->fin_v, fin->fin_ifp, fra->fra_info.fin_ifname);
	m->b_rptr -= qpi->qpi_off;
	fr_authpkts[i] = *(mblk_t **)fin->fin_mp;
# if !defined(_INET_IP_STACK_H)
	fra->fra_q = qpi->qpi_q;	/* The queue can disappear! */
# endif
	fra->fra_m = *fin->fin_mp;
	fra->fra_info.fin_mp = &fra->fra_m;
	cv_signal(&ipfauthwait);
	pollwakeup(&iplpollhead[IPL_LOGAUTH], POLLIN|POLLRDNORM);
#else
	fr_authpkts[i] = m;
	WAKEUP(&fr_authnext,0);
#endif
	return 1;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_auth_ioctl                                               */
/* Returns:     int - 0 == success, else error                              */
/* Parameters:  data(IO) - pointer to ioctl data                            */
/*              cmd(I)   - ioctl command                                    */
/*              mode(I)  - mode flags associated with open descriptor       */
/*              uid(I)   - uid associatd with application making the call   */
/*              ctx(I)   - pointer for context                              */
/*                                                                          */
/* This function handles all of the ioctls recognised by the auth component */
/* in IPFilter - ie ioctls called on an open fd for /dev/ipauth             */
/* ------------------------------------------------------------------------ */
int fr_auth_ioctl(data, cmd, mode, uid, ctx)
caddr_t data;
ioctlcmd_t cmd;
int mode, uid;
void *ctx;
{
	int error = 0, i;
	SPL_INT(s);

	switch (cmd)
	{
	case SIOCGENITER :
	    {
		ipftoken_t *token;
		ipfgeniter_t iter;

		error = fr_inobj(data, &iter, IPFOBJ_GENITER);
		if (error != 0)
			break;

		SPL_SCHED(s);
		token = ipf_findtoken(IPFGENITER_AUTH, uid, ctx);
		if (token != NULL)
			error = fr_authgeniter(token, &iter);
		else
			error = ESRCH;
		RWLOCK_EXIT(&ipf_tokens);
		SPL_X(s);

		break;
	    }

	case SIOCADAFR :
	case SIOCRMAFR :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(IPL_LOGAUTH, cmd, data,
					  fr_active, 1);
		break;

	case SIOCSTLCK :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		error = fr_lock(data, &fr_auth_lock);
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
		error = BCOPYOUT((char *)&i, data, sizeof(i));
		if (error != 0)
			error = EFAULT;
		break;

	case SIOCAUTHW:
		error = fr_authwait(data);
		break;

	case SIOCAUTHR:
		error = fr_authreply(data);
		break;

	default :
		error = EINVAL;
		break;
	}
	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_authunload                                               */
/* Returns:     None                                                        */
/* Parameters:  None                                                        */
/*                                                                          */
/* Free all network buffer memory used to keep saved packets.               */
/* ------------------------------------------------------------------------ */
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


/* ------------------------------------------------------------------------ */
/* Function:    fr_authexpire                                               */
/* Returns:     None                                                        */
/* Parameters:  None                                                        */
/*                                                                          */
/* Slowly expire held auth records.  Timeouts are set in expectation of     */
/* this being called twice per second.                                      */
/* ------------------------------------------------------------------------ */
void fr_authexpire()
{
	frauthent_t *fae, **faep;
	frentry_t *fr, **frp;
	frauth_t *fra;
	mb_t *m;
	int i;
	SPL_INT(s);

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

	/*
	 * Expire pre-auth rules
	 */
	for (faep = &fae_list; ((fae = *faep) != NULL); ) {
		fae->fae_age--;
		if (fae->fae_age == 0) {
			fr_authderef(&fae);
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


/* ------------------------------------------------------------------------ */
/* Function:    fr_preauthcmd                                               */
/* Returns:     int - 0 == success, else error                              */
/* Parameters:  cmd(I)  - ioctl command for rule                            */
/*              fr(I)   - pointer to ipf rule                               */
/*              fptr(I) - pointer to caller's 'fr'                          */
/*                                                                          */
/* ------------------------------------------------------------------------ */
int fr_preauthcmd(cmd, fr, frptr)
ioctlcmd_t cmd;
frentry_t *fr, **frptr;
{
	frauthent_t *fae, **faep;
	int error = 0;
	SPL_INT(s);

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
			fae->fae_ref = 1;
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


/* ------------------------------------------------------------------------ */
/* Function:    fr_authflush                                                */
/* Returns:     int - number of auth entries flushed                        */
/* Parameters:  None                                                        */
/* Locks:       WRITE(ipf_auth)                                             */
/*                                                                          */
/* This function flushs the fr_authpkts array of any packet data with       */
/* references still there.                                                  */
/* It is expected that the caller has already acquired the correct locks or */
/* set the priority level correctly for this to block out other code paths  */
/* into these data structures.                                              */
/* ------------------------------------------------------------------------ */
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


/* ------------------------------------------------------------------------ */
/* Function:    fr_auth_waiting                                             */
/* Returns:     int - 0 = no pakcets wiating, 1 = packets waiting.          */
/* Parameters:  None                                                        */
/*                                                                          */
/* Simple truth check to see if there are any packets waiting in the auth   */
/* queue.                                                                   */
/* ------------------------------------------------------------------------ */
int fr_auth_waiting()
{
	return (fr_authused != 0);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_authgeniter                                              */
/* Returns:     int - 0 == success, else error                              */
/* Parameters:  token(I) - pointer to ipftoken structure                    */
/*              itp(I)   - pointer to ipfgeniter structure                  */
/*                                                                          */
/* ------------------------------------------------------------------------ */
int fr_authgeniter(token, itp)
ipftoken_t *token;
ipfgeniter_t *itp;
{
	frauthent_t *fae, *next, zero;
	int error;

	if (itp->igi_data == NULL)
		return EFAULT;

	if (itp->igi_type != IPFGENITER_AUTH)
		return EINVAL;

	fae = token->ipt_data;
	READ_ENTER(&ipf_auth);
	if (fae == NULL) {
		next = fae_list;
	} else {
		next = fae->fae_next;
	}

	if (next != NULL) {
		/*
		 * If we find an auth entry to use, bump its reference count
		 * so that it can be used for is_next when we come back.
		 */
		ATOMIC_INC(next->fae_ref);
		if (next->fae_next == NULL) {
			ipf_freetoken(token);
			token = NULL;
		} else {
			token->ipt_data = next;
		}
	} else {
		bzero(&zero, sizeof(zero));
		next = &zero;
	}
	RWLOCK_EXIT(&ipf_auth);

	/*
	 * If we had a prior pointer to an auth entry, release it.
	 */
	if (fae != NULL) {
		WRITE_ENTER(&ipf_auth);
		fr_authderef(&fae);
		RWLOCK_EXIT(&ipf_auth);
	}

	/*
	 * This should arguably be via fr_outobj() so that the auth
	 * structure can (if required) be massaged going out.
	 */
	error = COPYOUT(next, itp->igi_data, sizeof(*next));
	if (error != 0)
		error = EFAULT;

	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_authderef                                                */
/* Returns:     None                                                        */
/* Parameters:  faep(IO) - pointer to caller's frauthent_t pointer          */
/* Locks:       WRITE(ipf_auth)                                             */
/*                                                                          */
/* This function unconditionally sets the pointer in the caller to NULL,    */
/* to make it clear that it should no longer use that pointer, and drops    */
/* the reference count on the structure by 1.  If it reaches 0, free it up. */
/* ------------------------------------------------------------------------ */
void fr_authderef(faep)
frauthent_t **faep;
{
	frauthent_t *fae;

	fae = *faep;
	*faep = NULL;

	fae->fae_ref--;
	if (fae->fae_ref == 0) {
		KFREE(fae);
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_authwait                                                 */
/* Returns:     int - 0 == success, else error                              */
/* Parameters:  data(I) - pointer to data from ioctl call                   */
/*                                                                          */
/* This function is called when an application is waiting for a packet to   */
/* match an "auth" rule by issuing an SIOCAUTHW ioctl.  If there is already */
/* a packet waiting on the queue then we will return that _one_ immediately.*/
/* If there are no packets present in the queue (fr_authpkts) then we go to */
/* sleep.                                                                   */
/* ------------------------------------------------------------------------ */
int fr_authwait(data)
char *data;
{
	frauth_t auth, *au = &auth;
	int error, len, i;
	mb_t *m;
	char *t;
#if defined(_KERNEL) && !defined(MENTAT) && !defined(linux) && \
    (!defined(__FreeBSD_version) || (__FreeBSD_version < 501000))
	SPL_INT(s);
#endif

fr_authioctlloop:
	error = fr_inobj(data, au, IPFOBJ_FRAUTH);
	if (error != 0)
		return error;

	/*
	 * XXX Locks are held below over calls to copyout...a better
	 * solution needs to be found so this isn't necessary.  The situation
	 * we are trying to guard against here is an error in the copyout
	 * steps should not cause the packet to "disappear" from the queue.
	 */
	READ_ENTER(&ipf_auth);

	/*
	 * If fr_authnext is not equal to fr_authend it will be because there
	 * is a packet waiting to be delt with in the fr_authpkts array.  We
	 * copy as much of that out to user space as requested.
	 */
	if (fr_authused > 0) {
		while (fr_authpkts[fr_authnext] == NULL) {
			fr_authnext++;
			if (fr_authnext == fr_authsize)
				fr_authnext = 0;
		}

		error = fr_outobj(data, &fr_auth[fr_authnext], IPFOBJ_FRAUTH);
		if (error != 0)
			return error;

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
				error = copyoutptr(MTOD(m, char *), &t, i);
				len -= i;
				t += i;
				if (error != 0)
					return error;
				m = m->m_next;
			}
		}
		RWLOCK_EXIT(&ipf_auth);

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
	if (error == 0)
		goto fr_authioctlloop;
	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_authreply                                                */
/* Returns:     int - 0 == success, else error                              */
/* Parameters:  data(I) - pointer to data from ioctl call                   */
/*                                                                          */
/* This function is called by an application when it wants to return a      */
/* decision on a packet using the SIOCAUTHR ioctl.  This is after it has    */
/* received information using an SIOCAUTHW.  The decision returned in the   */
/* form of flags, the same as those used in each rule.                      */
/* ------------------------------------------------------------------------ */
int fr_authreply(data)
char *data;
{
	frauth_t auth, *au = &auth, *fra;
	int error, i;
	mb_t *m;
	SPL_INT(s);

	error = fr_inobj(data, &auth, IPFOBJ_FRAUTH);
	if (error != 0)
		return error;

	SPL_NET(s);
	WRITE_ENTER(&ipf_auth);

	i = au->fra_index;
	fra = fr_auth + i;
	error = 0;

	/*
	 * Check the validity of the information being returned with two simple
	 * checks.  First, the auth index value should be within the size of
	 * the array and second the packet id being returned should also match.
	 */
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

	/*
	 * Re-insert the packet back into the packet stream flowing through
	 * the kernel in a manner that will mean IPFilter sees the packet
	 * again.  This is not the same as is done with fastroute,
	 * deliberately, as we want to resume the normal packet processing
	 * path for it.
	 */
#ifdef	_KERNEL
	if ((m != NULL) && (au->fra_info.fin_out != 0)) {
		error = ipf_inject(&fra->fra_info, m);
		if (error != 0) {
			error = ENOBUFS;
			fr_authstats.fas_sendfail++;
		} else {
			fr_authstats.fas_sendok++;
		}
	} else if (m) {
		error = ipf_inject(&fra->fra_info, m);
		if (error != 0) {
			error = ENOBUFS;
			fr_authstats.fas_quefail++;
		} else {
			fr_authstats.fas_queok++;
		}
	} else {
		error = EINVAL;
	}

	/*
	 * If we experience an error which will result in the packet
	 * not being processed, make sure we advance to the next one.
	 */
	if (error == ENOBUFS) {
		WRITE_ENTER(&ipf_auth);
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
		RWLOCK_EXIT(&ipf_auth);
	}
#endif /* _KERNEL */
	SPL_X(s);

	return 0;
}
