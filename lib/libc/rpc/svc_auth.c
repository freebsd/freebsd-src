/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc. 
 */

#ident	"@(#)svc_auth.c	1.16	94/04/24 SMI"

#if !defined(lint) && defined(SCCSIDS)
#if 0
static char sccsid[] = "@(#)svc_auth.c 1.26 89/02/07 Copyr 1984 Sun Micro";
#else
static const char rcsid[] =
 "$FreeBSD$";
#endif
#endif

/*
 * svc_auth.c, Server-side rpc authenticator interface.
 *
 */

#ifdef _KERNEL
#include <sys/param.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <rpc/svc.h>
#include <rpc/svc_auth.h>
#else
#include <stdlib.h>
#include <rpc/rpc.h>
#endif
#include <sys/types.h>

/*
 * svcauthsw is the bdevsw of server side authentication.
 *
 * Server side authenticators are called from authenticate by
 * using the client auth struct flavor field to index into svcauthsw.
 * The server auth flavors must implement a routine that looks
 * like:
 *
 *	enum auth_stat
 *	flavorx_auth(rqst, msg)
 *		register struct svc_req *rqst;
 *		register struct rpc_msg *msg;
 *
 */

enum auth_stat _svcauth_null();	/* no authentication */
enum auth_stat _svcauth_unix();		/* (system) unix style (uid, gids) */
enum auth_stat _svcauth_short();	/* short hand unix style */
enum auth_stat _svcauth_des();		/* des style */

/* declarations to allow servers to specify new authentication flavors */
struct authsvc {
	int	flavor;
	enum	auth_stat (*handler)();
	struct	authsvc	  *next;
};
static struct authsvc *Auths = NULL;

/*
 * The call rpc message, msg has been obtained from the wire.  The msg contains
 * the raw form of credentials and verifiers.  authenticate returns AUTH_OK
 * if the msg is successfully authenticated.  If AUTH_OK then the routine also
 * does the following things:
 * set rqst->rq_xprt->verf to the appropriate response verifier;
 * sets rqst->rq_client_cred to the "cooked" form of the credentials.
 *
 * NB: rqst->rq_cxprt->verf must be pre-alloctaed;
 * its length is set appropriately.
 *
 * The caller still owns and is responsible for msg->u.cmb.cred and
 * msg->u.cmb.verf.  The authentication system retains ownership of
 * rqst->rq_client_cred, the cooked credentials.
 *
 * There is an assumption that any flavour less than AUTH_NULL is
 * invalid.
 */
enum auth_stat
_authenticate(rqst, msg)
	register struct svc_req *rqst;
	struct rpc_msg *msg;
{
	register int cred_flavor;
	register struct authsvc *asp;

	rqst->rq_cred = msg->rm_call.cb_cred;
	rqst->rq_xprt->xp_verf.oa_flavor = _null_auth.oa_flavor;
	rqst->rq_xprt->xp_verf.oa_length = 0;
	cred_flavor = rqst->rq_cred.oa_flavor;
	switch (cred_flavor) {
	case AUTH_NULL:
		return(_svcauth_null(rqst, msg));
	case AUTH_UNIX:
		return(_svcauth_unix(rqst, msg));
	case AUTH_SHORT:
		return(_svcauth_short(rqst, msg));
	/*
	 * We leave AUTH_DES turned off by default because svcauth_des()
	 * needs getpublickey(), which is in librpcsvc, not libc. If we
 	 * included AUTH_DES as a built-in flavor, programs that don't
	 * have -lrpcsvc in their Makefiles wouldn't link correctly, even
	 * though they don't use AUTH_DES. And I'm too lazy to go through
	 * the tree looking for all of them.
	 */
#ifdef DES_BUILTIN
	case AUTH_DES:
		return(_svcauth_des(rqst, msg));
#endif
	}

	/* flavor doesn't match any of the builtin types, so try new ones */
	for (asp = Auths; asp; asp = asp->next) {
		if (asp->flavor == cred_flavor) {
			enum auth_stat as;

			as = (*asp->handler)(rqst, msg);
			return (as);
		}
	}

	return (AUTH_REJECTEDCRED);
}

/*ARGSUSED*/
enum auth_stat
_svcauth_null(rqst, msg)
	struct svc_req *rqst;
	struct rpc_msg *msg;
{
	return (AUTH_OK);
}

/*
 *  Allow the rpc service to register new authentication types that it is
 *  prepared to handle.  When an authentication flavor is registered,
 *  the flavor is checked against already registered values.  If not
 *  registered, then a new Auths entry is added on the list.
 *
 *  There is no provision to delete a registration once registered.
 *
 *  This routine returns:
 *	 0 if registration successful
 *	 1 if flavor already registered
 *	-1 if can't register (errno set)
 */

int
svc_auth_reg(cred_flavor, handler)
	register int cred_flavor;
	enum auth_stat (*handler)();
{
	register struct authsvc *asp;

	switch (cred_flavor) {
	    case AUTH_NULL:
	    case AUTH_UNIX:
	    case AUTH_SHORT:
#ifdef DES_BUILTIN
	    case AUTH_DES:
#endif
		/* already registered */
		return (1);

	    default:
		for (asp = Auths; asp; asp = asp->next) {
			if (asp->flavor == cred_flavor) {
				/* already registered */
				return (1);
			}
		}

		/* this is a new one, so go ahead and register it */
		asp = (struct authsvc *)mem_alloc(sizeof (*asp));
		if (asp == NULL) {
			return (-1);
		}
		asp->flavor = cred_flavor;
		asp->handler = handler;
		asp->next = Auths;
		Auths = asp;
		break;
	}
	return (0);
}
