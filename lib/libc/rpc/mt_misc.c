/*	$NetBSD: mt_misc.c,v 1.1 2000/06/02 23:11:11 fvdl Exp $	*/
/*	$FreeBSD$ */

/* #pragma ident	"@(#)mt_misc.c	1.24	93/04/29 SMI" */

#include "namespace.h"
#include "reentrant.h"
#include <rpc/rpc.h>
#include <sys/time.h>
#include <stdlib.h>
#include "un-namespace.h"

/* protects the services list (svc.c) */
pthread_rwlock_t	svc_lock = PTHREAD_RWLOCK_INITIALIZER;

/* protects svc_fdset and the xports[] array */
pthread_rwlock_t	svc_fd_lock = PTHREAD_RWLOCK_INITIALIZER;

/* protects the RPCBIND address cache */
pthread_rwlock_t	rpcbaddr_cache_lock = PTHREAD_RWLOCK_INITIALIZER;

/* protects authdes cache (svcauth_des.c) */
pthread_mutex_t	authdes_lock = PTHREAD_MUTEX_INITIALIZER;

/* serializes authdes ops initializations */
pthread_mutex_t authdes_ops_lock = PTHREAD_MUTEX_INITIALIZER;

/* protects des stats list */
pthread_mutex_t svcauthdesstats_lock = PTHREAD_MUTEX_INITIALIZER;

#ifdef KERBEROS
/* auth_kerb.c serialization */
pthread_mutex_t authkerb_lock = PTHREAD_MUTEX_INITIALIZER;
/* protects kerb stats list */
pthread_mutex_t svcauthkerbstats_lock = PTHREAD_MUTEX_INITIALIZER;
#endif /* KERBEROS */

/* auth_none.c serialization */
pthread_mutex_t	authnone_lock = PTHREAD_MUTEX_INITIALIZER;

/* protects the Auths list (svc_auth.c) */
pthread_mutex_t	authsvc_lock = PTHREAD_MUTEX_INITIALIZER;

/* protects client-side fd lock array */
pthread_mutex_t	clnt_fd_lock = PTHREAD_MUTEX_INITIALIZER;

/* clnt_raw.c serialization */
pthread_mutex_t	clntraw_lock = PTHREAD_MUTEX_INITIALIZER;

/* domainname and domain_fd (getdname.c) and default_domain (rpcdname.c) */
pthread_mutex_t	dname_lock = PTHREAD_MUTEX_INITIALIZER;

/* dupreq variables (svc_dg.c) */
pthread_mutex_t	dupreq_lock = PTHREAD_MUTEX_INITIALIZER;

/* protects first_time and hostname (key_call.c) */
pthread_mutex_t	keyserv_lock = PTHREAD_MUTEX_INITIALIZER;

/* serializes rpc_trace() (rpc_trace.c) */
pthread_mutex_t	libnsl_trace_lock = PTHREAD_MUTEX_INITIALIZER;

/* loopnconf (rpcb_clnt.c) */
pthread_mutex_t	loopnconf_lock = PTHREAD_MUTEX_INITIALIZER;

/* serializes ops initializations */
pthread_mutex_t	ops_lock = PTHREAD_MUTEX_INITIALIZER;

/* protects ``port'' static in bindresvport() */
pthread_mutex_t	portnum_lock = PTHREAD_MUTEX_INITIALIZER;

/* protects proglst list (svc_simple.c) */
pthread_mutex_t	proglst_lock = PTHREAD_MUTEX_INITIALIZER;

/* serializes clnt_com_create() (rpc_soc.c) */
pthread_mutex_t	rpcsoc_lock = PTHREAD_MUTEX_INITIALIZER;

/* svc_raw.c serialization */
pthread_mutex_t	svcraw_lock = PTHREAD_MUTEX_INITIALIZER;

/* protects TSD key creation */
pthread_mutex_t	tsd_lock = PTHREAD_MUTEX_INITIALIZER;

/* protects netconfig list */
pthread_mutex_t	nc_lock = PTHREAD_MUTEX_INITIALIZER;

/* xprtlist (svc_generic.c) */
pthread_mutex_t	xprtlist_lock = PTHREAD_MUTEX_INITIALIZER;

/* serializes calls to public key routines */
pthread_mutex_t serialize_pkey = PTHREAD_MUTEX_INITIALIZER;

#undef	rpc_createerr

struct rpc_createerr rpc_createerr;

struct rpc_createerr *
__rpc_createerr()
{
	static thread_key_t rce_key = 0;
	struct rpc_createerr *rce_addr = 0;

	if (thr_main())
		return (&rpc_createerr);
	if ((rce_addr =
	    (struct rpc_createerr *)thr_getspecific(rce_key)) != 0) {
		mutex_lock(&tsd_lock);
		if (thr_keycreate(&rce_key, free) != 0) {
			mutex_unlock(&tsd_lock);
			return (&rpc_createerr);
		}
		mutex_unlock(&tsd_lock);
	}
	if (!rce_addr) {
		rce_addr = (struct rpc_createerr *)
			malloc(sizeof (struct rpc_createerr));
		if (thr_setspecific(rce_key, (void *) rce_addr) != 0) {
			if (rce_addr)
				free(rce_addr);
			return (&rpc_createerr);
		}
		memset(rce_addr, 0, sizeof (struct rpc_createerr));
		return (rce_addr);
	}
	return (rce_addr);
}
