/*	$FreeBSD: src/sys/contrib/ipfilter/netinet/ip_sync.c,v 1.5.2.1 2007/10/31 05:00:38 darrenr Exp $	*/

/*
 * Copyright (C) 1995-1998 by Darren Reed.
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
#include <sys/file.h>
#if !defined(_KERNEL) && !defined(__KERNEL__)
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# define _KERNEL
# define KERNEL
# ifdef __OpenBSD__
struct file;
# endif
# include <sys/uio.h>
# undef _KERNEL
# undef KERNEL
#else
# include <sys/systm.h>
# if !defined(__SVR4) && !defined(__svr4__)
#  include <sys/mbuf.h>
# endif
#endif
#if defined(__NetBSD__) && (__NetBSD_Version__ >= 104000000)
# include <sys/proc.h>
#endif
#if defined(_KERNEL) && (__FreeBSD_version >= 220000)
# include <sys/filio.h>
# include <sys/fcntl.h>
# if (__FreeBSD_version >= 300000) && !defined(IPFILTER_LKM)
#  include "opt_ipfilter.h"
# endif
#else
# include <sys/ioctl.h>
#endif
#include <sys/time.h>
#if !defined(linux)
# include <sys/protosw.h>
#endif
#include <sys/socket.h>
#if defined(__SVR4) || defined(__svr4__)
# include <sys/filio.h>
# include <sys/byteorder.h>
# ifdef _KERNEL
#  include <sys/dditypes.h>
# endif
# include <sys/stream.h>
# include <sys/kmem.h>
#endif

#include <net/if.h>
#ifdef sun
# include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#if !defined(linux)
# include <netinet/ip_var.h>
#endif
#if !defined(__hpux) && !defined(linux)
# include <netinet/tcp_fsm.h>
#endif
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#include <netinet/tcpip.h>
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_sync.h"
#ifdef  USE_INET6
#include <netinet/icmp6.h>
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
static const char rcsid[] = "@(#)$Id: ip_sync.c,v 2.40.2.9 2007/06/02 21:22:28 darrenr Exp $";
#endif

#define	SYNC_STATETABSZ	256
#define	SYNC_NATTABSZ	256

#ifdef	IPFILTER_SYNC
ipfmutex_t	ipf_syncadd, ipsl_mutex;
ipfrwlock_t	ipf_syncstate, ipf_syncnat;
#if SOLARIS && defined(_KERNEL)
kcondvar_t	ipslwait;
#endif
synclist_t	*syncstatetab[SYNC_STATETABSZ];
synclist_t	*syncnattab[SYNC_NATTABSZ];
synclogent_t	synclog[SYNCLOG_SZ];
syncupdent_t	syncupd[SYNCLOG_SZ];
u_int		ipf_syncnum = 1;
u_int		ipf_syncwrap = 0;
u_int		sl_idx = 0,	/* next available sync log entry */
		su_idx = 0,	/* next available sync update entry */
		sl_tail = 0,	/* next sync log entry to read */
		su_tail = 0;	/* next sync update entry to read */
int		ipf_sync_debug = 0;


# if !defined(sparc) && !defined(__hppa)
void ipfsync_tcporder __P((int, struct tcpdata *));
void ipfsync_natorder __P((int, struct nat *));
void ipfsync_storder __P((int, struct ipstate *));
# endif


/* ------------------------------------------------------------------------ */
/* Function:    ipfsync_init                                                */
/* Returns:     int - 0 == success, -1 == failure                           */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Initialise all of the locks required for the sync code and initialise    */
/* any data structures, as required.                                        */
/* ------------------------------------------------------------------------ */
int ipfsync_init()
{
	RWLOCK_INIT(&ipf_syncstate, "add things to state sync table");
	RWLOCK_INIT(&ipf_syncnat, "add things to nat sync table");
	MUTEX_INIT(&ipf_syncadd, "add things to sync table");
	MUTEX_INIT(&ipsl_mutex, "add things to sync table");
# if SOLARIS && defined(_KERNEL)
	cv_init(&ipslwait, "ipsl condvar", CV_DRIVER, NULL);
# endif

	bzero((char *)syncnattab, sizeof(syncnattab));
	bzero((char *)syncstatetab, sizeof(syncstatetab));

	return 0;
}


# if !defined(sparc) && !defined(__hppa)
/* ------------------------------------------------------------------------ */
/* Function:    ipfsync_tcporder                                            */
/* Returns:     Nil                                                         */
/* Parameters:  way(I) - direction of byte order conversion.                */
/*              td(IO) - pointer to data to be converted.                   */
/*                                                                          */
/* Do byte swapping on values in the TCP state information structure that   */
/* need to be used at both ends by the host in their native byte order.     */
/* ------------------------------------------------------------------------ */
void ipfsync_tcporder(way, td)
int way;
tcpdata_t *td;
{
	if (way) {
		td->td_maxwin = htons(td->td_maxwin);
		td->td_end = htonl(td->td_end);
		td->td_maxend = htonl(td->td_maxend);
	} else {
		td->td_maxwin = ntohs(td->td_maxwin);
		td->td_end = ntohl(td->td_end);
		td->td_maxend = ntohl(td->td_maxend);
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    ipfsync_natorder                                            */
/* Returns:     Nil                                                         */
/* Parameters:  way(I)  - direction of byte order conversion.               */
/*              nat(IO) - pointer to data to be converted.                  */
/*                                                                          */
/* Do byte swapping on values in the NAT data structure that need to be     */
/* used at both ends by the host in their native byte order.                */
/* ------------------------------------------------------------------------ */
void ipfsync_natorder(way, n)
int way;
nat_t *n;
{
	if (way) {
		n->nat_age = htonl(n->nat_age);
		n->nat_flags = htonl(n->nat_flags);
		n->nat_ipsumd = htonl(n->nat_ipsumd);
		n->nat_use = htonl(n->nat_use);
		n->nat_dir = htonl(n->nat_dir);
	} else {
		n->nat_age = ntohl(n->nat_age);
		n->nat_flags = ntohl(n->nat_flags);
		n->nat_ipsumd = ntohl(n->nat_ipsumd);
		n->nat_use = ntohl(n->nat_use);
		n->nat_dir = ntohl(n->nat_dir);
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    ipfsync_storder                                             */
/* Returns:     Nil                                                         */
/* Parameters:  way(I)  - direction of byte order conversion.               */
/*              ips(IO) - pointer to data to be converted.                  */
/*                                                                          */
/* Do byte swapping on values in the IP state data structure that need to   */
/* be used at both ends by the host in their native byte order.             */
/* ------------------------------------------------------------------------ */
void ipfsync_storder(way, ips)
int way;
ipstate_t *ips;
{
	ipfsync_tcporder(way, &ips->is_tcp.ts_data[0]);
	ipfsync_tcporder(way, &ips->is_tcp.ts_data[1]);

	if (way) {
		ips->is_hv = htonl(ips->is_hv);
		ips->is_die = htonl(ips->is_die);
		ips->is_pass = htonl(ips->is_pass);
		ips->is_flags = htonl(ips->is_flags);
		ips->is_opt[0] = htonl(ips->is_opt[0]);
		ips->is_opt[1] = htonl(ips->is_opt[1]);
		ips->is_optmsk[0] = htonl(ips->is_optmsk[0]);
		ips->is_optmsk[1] = htonl(ips->is_optmsk[1]);
		ips->is_sec = htons(ips->is_sec);
		ips->is_secmsk = htons(ips->is_secmsk);
		ips->is_auth = htons(ips->is_auth);
		ips->is_authmsk = htons(ips->is_authmsk);
		ips->is_s0[0] = htonl(ips->is_s0[0]);
		ips->is_s0[1] = htonl(ips->is_s0[1]);
		ips->is_smsk[0] = htons(ips->is_smsk[0]);
		ips->is_smsk[1] = htons(ips->is_smsk[1]);
	} else {
		ips->is_hv = ntohl(ips->is_hv);
		ips->is_die = ntohl(ips->is_die);
		ips->is_pass = ntohl(ips->is_pass);
		ips->is_flags = ntohl(ips->is_flags);
		ips->is_opt[0] = ntohl(ips->is_opt[0]);
		ips->is_opt[1] = ntohl(ips->is_opt[1]);
		ips->is_optmsk[0] = ntohl(ips->is_optmsk[0]);
		ips->is_optmsk[1] = ntohl(ips->is_optmsk[1]);
		ips->is_sec = ntohs(ips->is_sec);
		ips->is_secmsk = ntohs(ips->is_secmsk);
		ips->is_auth = ntohs(ips->is_auth);
		ips->is_authmsk = ntohs(ips->is_authmsk);
		ips->is_s0[0] = ntohl(ips->is_s0[0]);
		ips->is_s0[1] = ntohl(ips->is_s0[1]);
		ips->is_smsk[0] = ntohl(ips->is_smsk[0]);
		ips->is_smsk[1] = ntohl(ips->is_smsk[1]);
	}
}
# else /* !defined(sparc) && !defined(__hppa) */
#  define	ipfsync_tcporder(x,y)
#  define	ipfsync_natorder(x,y)
#  define	ipfsync_storder(x,y)
# endif /* !defined(sparc) && !defined(__hppa) */

/* enable this for debugging */

# ifdef _KERNEL
/* ------------------------------------------------------------------------ */
/* Function:    ipfsync_write                                               */
/* Returns:     int    - 0 == success, else error value.                    */
/* Parameters:  uio(I) - pointer to information about data to write         */
/*                                                                          */
/* Moves data from user space into the kernel and uses it for updating data */
/* structures in the state/NAT tables.                                      */
/* ------------------------------------------------------------------------ */
int ipfsync_write(uio)
struct uio *uio;
{
	synchdr_t sh;

	/* 
	 * THIS MUST BE SUFFICIENT LARGE TO STORE
	 * ANY POSSIBLE DATA TYPE 
	 */
	char data[2048]; 

	int err = 0;

#  if (BSD >= 199306) || defined(__FreeBSD__) || defined(__osf__)
	uio->uio_rw = UIO_WRITE;
#  endif

	/* Try to get bytes */
	while (uio->uio_resid > 0) {

		if (uio->uio_resid >= sizeof(sh)) {

			err = UIOMOVE(&sh, sizeof(sh), UIO_WRITE, uio);

			if (err) {
				if (ipf_sync_debug > 2)
					printf("uiomove(header) failed: %d\n",
						err);
				return err;
			}

			/* convert to host order */
			sh.sm_magic = ntohl(sh.sm_magic);
			sh.sm_len = ntohl(sh.sm_len);
			sh.sm_num = ntohl(sh.sm_num);

			if (ipf_sync_debug > 8)
				printf("[%d] Read v:%d p:%d cmd:%d table:%d rev:%d len:%d magic:%x\n",
					sh.sm_num, sh.sm_v, sh.sm_p, sh.sm_cmd,
					sh.sm_table, sh.sm_rev, sh.sm_len,
					sh.sm_magic);

			if (sh.sm_magic != SYNHDRMAGIC) {
				if (ipf_sync_debug > 2)
					printf("uiomove(header) invalud %s\n",
						"magic");
				return EINVAL;
			}

			if (sh.sm_v != 4 && sh.sm_v != 6) {
				if (ipf_sync_debug > 2)
					printf("uiomove(header) invalid %s\n",
						"protocol");
				return EINVAL;
			}

			if (sh.sm_cmd > SMC_MAXCMD) {
				if (ipf_sync_debug > 2)
					printf("uiomove(header) invalid %s\n",
						"command");
				return EINVAL;
			}


			if (sh.sm_table > SMC_MAXTBL) {
				if (ipf_sync_debug > 2)
					printf("uiomove(header) invalid %s\n",
						"table");
				return EINVAL;
			}

		} else {
			/* unsufficient data, wait until next call */
			if (ipf_sync_debug > 2)
				printf("uiomove(header) insufficient data");
			return EAGAIN;
	 	}


		/*
		 * We have a header, so try to read the amount of data 
		 * needed for the request
		 */

		/* not supported */
		if (sh.sm_len == 0) {
			if (ipf_sync_debug > 2)
				printf("uiomove(data zero length %s\n",
					"not supported");
			return EINVAL;
		}

		if (uio->uio_resid >= sh.sm_len) {

			err = UIOMOVE(data, sh.sm_len, UIO_WRITE, uio);

			if (err) {
				if (ipf_sync_debug > 2)
					printf("uiomove(data) failed: %d\n",
						err);
				return err;
			}

			if (ipf_sync_debug > 7)
				printf("uiomove(data) %d bytes read\n",
					sh.sm_len);

			if (sh.sm_table == SMC_STATE)
				err = ipfsync_state(&sh, data);
			else if (sh.sm_table == SMC_NAT)
				err = ipfsync_nat(&sh, data);
			if (ipf_sync_debug > 7)
				printf("[%d] Finished with error %d\n",
					sh.sm_num, err);

		} else {
			/* insufficient data, wait until next call */
			if (ipf_sync_debug > 2)
				printf("uiomove(data) %s %d bytes, got %d\n",
					"insufficient data, need",
					sh.sm_len, uio->uio_resid);
			return EAGAIN;
		}
	}	 

	/* no more data */
	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipfsync_read                                                */
/* Returns:     int    - 0 == success, else error value.                    */
/* Parameters:  uio(O) - pointer to information about where to store data   */
/*                                                                          */
/* This function is called when a user program wants to read some data      */
/* for pending state/NAT updates.  If no data is available, the caller is   */
/* put to sleep, pending a wakeup from the "lower half" of this code.       */
/* ------------------------------------------------------------------------ */
int ipfsync_read(uio)
struct uio *uio;
{
	syncupdent_t *su;
	synclogent_t *sl;
	int err = 0;

	if ((uio->uio_resid & 3) || (uio->uio_resid < 8))
		return EINVAL;

#  if (BSD >= 199306) || defined(__FreeBSD__) || defined(__osf__)
	uio->uio_rw = UIO_READ;
#  endif

	MUTEX_ENTER(&ipsl_mutex);
	while ((sl_tail == sl_idx) && (su_tail == su_idx)) {
#  if SOLARIS && defined(_KERNEL)
		if (!cv_wait_sig(&ipslwait, &ipsl_mutex)) {
			MUTEX_EXIT(&ipsl_mutex);
			return EINTR;
		}
#  else
#   ifdef __hpux
		{
		lock_t *l;

		l = get_sleep_lock(&sl_tail);
		err = sleep(&sl_tail, PZERO+1);
		if (err) {
			MUTEX_EXIT(&ipsl_mutex);
			return EINTR;
		}
		spinunlock(l);
		}
#   else /* __hpux */
#    ifdef __osf__
		err = mpsleep(&sl_tail, PSUSP|PCATCH,  "ipl sleep", 0,
			      &ipsl_mutex, MS_LOCK_SIMPLE);
		if (err)
			return EINTR;
#    else
		MUTEX_EXIT(&ipsl_mutex);
		err = SLEEP(&sl_tail, "ipl sleep");
		if (err)
			return EINTR;
		MUTEX_ENTER(&ipsl_mutex);
#    endif /* __osf__ */
#   endif /* __hpux */
#  endif /* SOLARIS */
	}
	MUTEX_EXIT(&ipsl_mutex);

	READ_ENTER(&ipf_syncstate);
	while ((sl_tail < sl_idx)  && (uio->uio_resid > sizeof(*sl))) {
		sl = synclog + sl_tail++;
		err = UIOMOVE(sl, sizeof(*sl), UIO_READ, uio);
		if (err != 0)
			break;
	}

	while ((su_tail < su_idx)  && (uio->uio_resid > sizeof(*su))) {
		su = syncupd + su_tail;
		su_tail++;
		err = UIOMOVE(su, sizeof(*su), UIO_READ, uio);
		if (err != 0)
			break;
		if (su->sup_hdr.sm_sl != NULL)
			su->sup_hdr.sm_sl->sl_idx = -1;
	}

	MUTEX_ENTER(&ipf_syncadd);
	if (su_tail == su_idx)
		su_tail = su_idx = 0;
	if (sl_tail == sl_idx)
		sl_tail = sl_idx = 0;
	MUTEX_EXIT(&ipf_syncadd);
	RWLOCK_EXIT(&ipf_syncstate);
	return err;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipfsync_state                                               */
/* Returns:     int    - 0 == success, else error value.                    */
/* Parameters:  sp(I)  - pointer to sync packet data header                 */
/*              uio(I) - pointer to user data for further information       */
/*                                                                          */
/* Updates the state table according to information passed in the sync      */
/* header.  As required, more data is fetched from the uio structure but    */
/* varies depending on the contents of the sync header.  This function can  */
/* create a new state entry or update one.  Deletion is left to the state   */
/* structures being timed out correctly.                                    */
/* ------------------------------------------------------------------------ */
int ipfsync_state(sp, data)
synchdr_t *sp;
void *data;
{
	synctcp_update_t su;
	ipstate_t *is, sn;
	synclist_t *sl;
	frentry_t *fr;
	u_int hv;
	int err = 0;

	hv = sp->sm_num & (SYNC_STATETABSZ - 1);

	switch (sp->sm_cmd)
	{
	case SMC_CREATE :

		bcopy(data, &sn, sizeof(sn));
		KMALLOC(is, ipstate_t *);
		if (is == NULL) {
			err = ENOMEM;
			break;
		}

		KMALLOC(sl, synclist_t *);
		if (sl == NULL) {
			err = ENOMEM;
			KFREE(is);
			break;
		}

		bzero((char *)is, offsetof(ipstate_t, is_die));
		bcopy((char *)&sn.is_die, (char *)&is->is_die,
		      sizeof(*is) - offsetof(ipstate_t, is_die));
		ipfsync_storder(0, is);

		/*
		 * We need to find the same rule on the slave as was used on
		 * the master to create this state entry.
		 */
		READ_ENTER(&ipf_mutex);
		fr = fr_getrulen(IPL_LOGIPF, sn.is_group, sn.is_rulen);
		if (fr != NULL) {
			MUTEX_ENTER(&fr->fr_lock);
			fr->fr_ref++;
			fr->fr_statecnt++;
			MUTEX_EXIT(&fr->fr_lock);
		}
		RWLOCK_EXIT(&ipf_mutex);

		if (ipf_sync_debug > 4)
			printf("[%d] Filter rules = %p\n", sp->sm_num, fr);

		is->is_rule = fr;
		is->is_sync = sl;

		sl->sl_idx = -1;
		sl->sl_ips = is;
		bcopy(sp, &sl->sl_hdr, sizeof(struct synchdr));

		WRITE_ENTER(&ipf_syncstate);
		WRITE_ENTER(&ipf_state);

		sl->sl_pnext = syncstatetab + hv;
		sl->sl_next = syncstatetab[hv];
		if (syncstatetab[hv] != NULL)
			syncstatetab[hv]->sl_pnext = &sl->sl_next;
		syncstatetab[hv] = sl;
		MUTEX_DOWNGRADE(&ipf_syncstate);
		fr_stinsert(is, sp->sm_rev);
		/*
		 * Do not initialise the interface pointers for the state
		 * entry as the full complement of interface names may not
		 * be present.
		 *
		 * Put this state entry on its timeout queue.
		 */
		/*fr_setstatequeue(is, sp->sm_rev);*/
		break;

	case SMC_UPDATE :
		bcopy(data, &su, sizeof(su));

		if (ipf_sync_debug > 4)
			printf("[%d] Update age %lu state %d/%d \n",
				sp->sm_num, su.stu_age, su.stu_state[0],
				su.stu_state[1]);

		READ_ENTER(&ipf_syncstate);
		for (sl = syncstatetab[hv]; (sl != NULL); sl = sl->sl_next)
			if (sl->sl_hdr.sm_num == sp->sm_num)
				break;
		if (sl == NULL) {
			if (ipf_sync_debug > 1)
				printf("[%d] State not found - can't update\n",
					sp->sm_num);
			RWLOCK_EXIT(&ipf_syncstate);
			err = ENOENT;
			break;
		}

		READ_ENTER(&ipf_state);

		if (ipf_sync_debug > 6)
			printf("[%d] Data from state v:%d p:%d cmd:%d table:%d rev:%d\n", 
				sp->sm_num, sl->sl_hdr.sm_v, sl->sl_hdr.sm_p, 
				sl->sl_hdr.sm_cmd, sl->sl_hdr.sm_table,
				sl->sl_hdr.sm_rev);

		is = sl->sl_ips;

		MUTEX_ENTER(&is->is_lock);
		switch (sp->sm_p)
		{
		case IPPROTO_TCP :
			/* XXX FV --- shouldn't we do ntohl/htonl???? XXX */
			is->is_send = su.stu_data[0].td_end;
			is->is_maxsend = su.stu_data[0].td_maxend;
			is->is_maxswin = su.stu_data[0].td_maxwin;
			is->is_state[0] = su.stu_state[0];
			is->is_dend = su.stu_data[1].td_end;
			is->is_maxdend = su.stu_data[1].td_maxend;
			is->is_maxdwin = su.stu_data[1].td_maxwin;
			is->is_state[1] = su.stu_state[1];
			break;
		default :
			break;
		}

		if (ipf_sync_debug > 6)
			printf("[%d] Setting timers for state\n", sp->sm_num);

		fr_setstatequeue(is, sp->sm_rev);

		MUTEX_EXIT(&is->is_lock);
		break;

	default :
		err = EINVAL;
		break;
	}

	if (err == 0) {
		RWLOCK_EXIT(&ipf_state);
		RWLOCK_EXIT(&ipf_syncstate);
	}

	if (ipf_sync_debug > 6)
		printf("[%d] Update completed with error %d\n",
			sp->sm_num, err);

	return err;
}
# endif /* _KERNEL */


/* ------------------------------------------------------------------------ */
/* Function:    ipfsync_del                                                 */
/* Returns:     Nil                                                         */
/* Parameters:  sl(I) - pointer to synclist object to delete                */
/*                                                                          */
/* Deletes an object from the synclist table and free's its memory.         */
/* ------------------------------------------------------------------------ */
void ipfsync_del(sl)
synclist_t *sl;
{
	WRITE_ENTER(&ipf_syncstate);
	*sl->sl_pnext = sl->sl_next;
	if (sl->sl_next != NULL)
		sl->sl_next->sl_pnext = sl->sl_pnext;
	if (sl->sl_idx != -1)
		syncupd[sl->sl_idx].sup_hdr.sm_sl = NULL;
	RWLOCK_EXIT(&ipf_syncstate);
	KFREE(sl);
}


/* ------------------------------------------------------------------------ */
/* Function:    ipfsync_nat                                                 */
/* Returns:     int    - 0 == success, else error value.                    */
/* Parameters:  sp(I)  - pointer to sync packet data header                 */
/*              uio(I) - pointer to user data for further information       */
/*                                                                          */
/* Updates the NAT  table according to information passed in the sync       */
/* header.  As required, more data is fetched from the uio structure but    */
/* varies depending on the contents of the sync header.  This function can  */
/* create a new NAT entry or update one.  Deletion is left to the NAT       */
/* structures being timed out correctly.                                    */
/* ------------------------------------------------------------------------ */
int ipfsync_nat(sp, data)
synchdr_t *sp;
void *data;
{
	syncupdent_t su;
	nat_t *n, *nat;
	synclist_t *sl;
	u_int hv = 0;
	int err;

	READ_ENTER(&ipf_syncstate);

	switch (sp->sm_cmd)
	{
	case SMC_CREATE :
		KMALLOC(n, nat_t *);
		if (n == NULL) {
			err = ENOMEM;
			break;
		}

		KMALLOC(sl, synclist_t *);
		if (sl == NULL) {
			err = ENOMEM;
			KFREE(n);
			break;
		}

		nat = (nat_t *)data;
		bzero((char *)n, offsetof(nat_t, nat_age));
		bcopy((char *)&nat->nat_age, (char *)&n->nat_age,
		      sizeof(*n) - offsetof(nat_t, nat_age));
		ipfsync_natorder(0, n);
		n->nat_sync = sl;

		sl->sl_idx = -1;
		sl->sl_ipn = n;
		sl->sl_num = ntohl(sp->sm_num);

		WRITE_ENTER(&ipf_nat);
		sl->sl_pnext = syncstatetab + hv;
		sl->sl_next = syncstatetab[hv];
		if (syncstatetab[hv] != NULL)
			syncstatetab[hv]->sl_pnext = &sl->sl_next;
		syncstatetab[hv] = sl;
		nat_insert(n, sl->sl_rev);
		RWLOCK_EXIT(&ipf_nat);
		break;

	case SMC_UPDATE :
		bcopy(data, &su, sizeof(su));

		READ_ENTER(&ipf_syncstate);
		for (sl = syncstatetab[hv]; (sl != NULL); sl = sl->sl_next)
			if (sl->sl_hdr.sm_num == sp->sm_num)
				break;
		if (sl == NULL) {
			err = ENOENT;
			break;
		}

		READ_ENTER(&ipf_nat);

		nat = sl->sl_ipn;

		MUTEX_ENTER(&nat->nat_lock);
		fr_setnatqueue(nat, sl->sl_rev);
		MUTEX_EXIT(&nat->nat_lock);

		RWLOCK_EXIT(&ipf_nat);

		break;

	default :
		err = EINVAL;
		break;
	}

	RWLOCK_EXIT(&ipf_syncstate);
	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipfsync_new                                                 */
/* Returns:     synclist_t* - NULL == failure, else pointer to new synclist */
/*                            data structure.                               */
/* Parameters:  tab(I) - type of synclist_t to create                       */
/*              fin(I) - pointer to packet information                      */
/*              ptr(I) - pointer to owning object                           */
/*                                                                          */
/* Creates a new sync table entry and notifies any sleepers that it's there */
/* waiting to be processed.                                                 */
/* ------------------------------------------------------------------------ */
synclist_t *ipfsync_new(tab, fin, ptr)
int tab;
fr_info_t *fin;
void *ptr;
{
	synclist_t *sl, *ss;
	synclogent_t *sle;
	u_int hv, sz;

	if (sl_idx == SYNCLOG_SZ)
		return NULL;
	KMALLOC(sl, synclist_t *);
	if (sl == NULL)
		return NULL;

	MUTEX_ENTER(&ipf_syncadd);
	/*
	 * Get a unique number for this synclist_t.  The number is only meant
	 * to be unique for the lifetime of the structure and may be reused
	 * later.
	 */
	ipf_syncnum++;
	if (ipf_syncnum == 0) {
		ipf_syncnum = 1;
		ipf_syncwrap = 1;
	}

	hv = ipf_syncnum & (SYNC_STATETABSZ - 1);
	while (ipf_syncwrap != 0) {
		for (ss = syncstatetab[hv]; ss; ss = ss->sl_next)
			if (ss->sl_hdr.sm_num == ipf_syncnum)
				break;
		if (ss == NULL)
			break;
		ipf_syncnum++;
		hv = ipf_syncnum & (SYNC_STATETABSZ - 1);
	}
	/*
	 * Use the synch number of the object as the hash key.  Should end up
	 * with relatively even distribution over time.
	 * XXX - an attacker could lunch an DoS attack, of sorts, if they are
	 * the only one causing new table entries by only keeping open every
	 * nth connection they make, where n is a value in the interval
	 * [0, SYNC_STATETABSZ-1].
	 */
	sl->sl_pnext = syncstatetab + hv;
	sl->sl_next = syncstatetab[hv];
	syncstatetab[hv] = sl;
	sl->sl_num = ipf_syncnum;
	MUTEX_EXIT(&ipf_syncadd);

	sl->sl_magic = htonl(SYNHDRMAGIC);
	sl->sl_v = fin->fin_v;
	sl->sl_p = fin->fin_p;
	sl->sl_cmd = SMC_CREATE;
	sl->sl_idx = -1;
	sl->sl_table = tab;
	sl->sl_rev = fin->fin_rev;
	if (tab == SMC_STATE) {
		sl->sl_ips = ptr;
		sz = sizeof(*sl->sl_ips);
	} else if (tab == SMC_NAT) {
		sl->sl_ipn = ptr;
		sz = sizeof(*sl->sl_ipn);
	} else {
		ptr = NULL;
		sz = 0;
	}
	sl->sl_len = sz;

	/*
	 * Create the log entry to be read by a user daemon.  When it has been
	 * finished and put on the queue, send a signal to wakeup any waiters.
	 */
	MUTEX_ENTER(&ipf_syncadd);
	sle = synclog + sl_idx++;
	bcopy((char *)&sl->sl_hdr, (char *)&sle->sle_hdr,
	      sizeof(sle->sle_hdr));
	sle->sle_hdr.sm_num = htonl(sle->sle_hdr.sm_num);
	sle->sle_hdr.sm_len = htonl(sle->sle_hdr.sm_len);
	if (ptr != NULL) {
		bcopy((char *)ptr, (char *)&sle->sle_un, sz);
		if (tab == SMC_STATE) {
			ipfsync_storder(1, &sle->sle_un.sleu_ips);
		} else if (tab == SMC_NAT) {
			ipfsync_natorder(1, &sle->sle_un.sleu_ipn);
		}
	}
	MUTEX_EXIT(&ipf_syncadd);

	MUTEX_ENTER(&ipsl_mutex);
# if SOLARIS
#  ifdef _KERNEL
	cv_signal(&ipslwait);
#  endif
	MUTEX_EXIT(&ipsl_mutex);
# else
	MUTEX_EXIT(&ipsl_mutex);
#  ifdef _KERNEL
	wakeup(&sl_tail);
#  endif
# endif
	return sl;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipfsync_update                                              */
/* Returns:     Nil                                                         */
/* Parameters:  tab(I) - type of synclist_t to create                       */
/*              fin(I) - pointer to packet information                      */
/*              sl(I)  - pointer to synchronisation object                  */
/*                                                                          */
/* For outbound packets, only, create an sync update record for the user    */
/* process to read.                                                         */
/* ------------------------------------------------------------------------ */
void ipfsync_update(tab, fin, sl)
int tab;
fr_info_t *fin;
synclist_t *sl;
{
	synctcp_update_t *st;
	syncupdent_t *slu;
	ipstate_t *ips;
	nat_t *nat;

	if (fin->fin_out == 0 || sl == NULL)
		return;

	WRITE_ENTER(&ipf_syncstate);
	MUTEX_ENTER(&ipf_syncadd);
	if (sl->sl_idx == -1) {
		slu = syncupd + su_idx;
		sl->sl_idx = su_idx++;
		bcopy((char *)&sl->sl_hdr, (char *)&slu->sup_hdr,
		      sizeof(slu->sup_hdr));
		slu->sup_hdr.sm_magic = htonl(SYNHDRMAGIC);
		slu->sup_hdr.sm_sl = sl;
		slu->sup_hdr.sm_cmd = SMC_UPDATE;
		slu->sup_hdr.sm_table = tab;
		slu->sup_hdr.sm_num = htonl(sl->sl_num);
		slu->sup_hdr.sm_len = htonl(sizeof(struct synctcp_update));
		slu->sup_hdr.sm_rev = fin->fin_rev;
# if 0
		if (fin->fin_p == IPPROTO_TCP) {
			st->stu_len[0] = 0;
			st->stu_len[1] = 0;
		}
# endif
	} else
		slu = syncupd + sl->sl_idx;
	MUTEX_EXIT(&ipf_syncadd);
	MUTEX_DOWNGRADE(&ipf_syncstate);

	/*
	 * Only TCP has complex timeouts, others just use default timeouts.
	 * For TCP, we only need to track the connection state and window.
	 */
	if (fin->fin_p == IPPROTO_TCP) {
		st = &slu->sup_tcp;
		if (tab == SMC_STATE) {
			ips = sl->sl_ips;
			st->stu_age = htonl(ips->is_die);
			st->stu_data[0].td_end = ips->is_send;
			st->stu_data[0].td_maxend = ips->is_maxsend;
			st->stu_data[0].td_maxwin = ips->is_maxswin;
			st->stu_state[0] = ips->is_state[0];
			st->stu_data[1].td_end = ips->is_dend;
			st->stu_data[1].td_maxend = ips->is_maxdend;
			st->stu_data[1].td_maxwin = ips->is_maxdwin;
			st->stu_state[1] = ips->is_state[1];
		} else if (tab == SMC_NAT) {
			nat = sl->sl_ipn;
			st->stu_age = htonl(nat->nat_age);
		}
	}
	RWLOCK_EXIT(&ipf_syncstate);

	MUTEX_ENTER(&ipsl_mutex);
# if SOLARIS
#  ifdef _KERNEL
	cv_signal(&ipslwait);
#  endif
	MUTEX_EXIT(&ipsl_mutex);
# else
	MUTEX_EXIT(&ipsl_mutex);
#  ifdef _KERNEL
	wakeup(&sl_tail);
#  endif
# endif
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_sync_ioctl                                               */
/* Returns:     int - 0 == success, != 0 == failure                         */
/* Parameters:  data(I) - pointer to ioctl data                             */
/*              cmd(I)  - ioctl command integer                             */
/*              mode(I) - file mode bits used with open                     */
/*                                                                          */
/* This function currently does not handle any ioctls and so just returns   */
/* EINVAL on all occasions.                                                 */
/* ------------------------------------------------------------------------ */
int fr_sync_ioctl(data, cmd, mode, uid, ctx)
caddr_t data;
ioctlcmd_t cmd;
int mode, uid;
void *ctx;
{
	return EINVAL;
}


int ipfsync_canread()
{
	return !((sl_tail == sl_idx) && (su_tail == su_idx));
}


int ipfsync_canwrite()
{
	return 1;
}
#endif /* IPFILTER_SYNC */
