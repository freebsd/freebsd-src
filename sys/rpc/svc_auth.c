/*	$NetBSD: svc_auth.c,v 1.12 2000/07/06 03:10:35 christos Exp $	*/

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

#if defined(LIBC_SCCS) && !defined(lint)
#ident	"@(#)svc_auth.c	1.16	94/04/24 SMI"
static char sccsid[] = "@(#)svc_auth.c 1.26 89/02/07 Copyr 1984 Sun Micro";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/rpc/svc_auth.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * svc_auth.c, Server-side rpc authenticator interface.
 *
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/ucred.h>

#include <rpc/rpc.h>

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
_authenticate(struct svc_req *rqst, struct rpc_msg *msg)
{
	int cred_flavor;
	enum auth_stat dummy;

	rqst->rq_cred = msg->rm_call.cb_cred;
	rqst->rq_xprt->xp_verf.oa_flavor = _null_auth.oa_flavor;
	rqst->rq_xprt->xp_verf.oa_length = 0;
	cred_flavor = rqst->rq_cred.oa_flavor;
	switch (cred_flavor) {
	case AUTH_NULL:
		dummy = _svcauth_null(rqst, msg);
		return (dummy);
	case AUTH_SYS:
		dummy = _svcauth_unix(rqst, msg);
		return (dummy);
	case AUTH_SHORT:
		dummy = _svcauth_short(rqst, msg);
		return (dummy);
	default:
		break;
	}

	return (AUTH_REJECTEDCRED);
}

/*ARGSUSED*/
enum auth_stat
_svcauth_null(struct svc_req *rqst, struct rpc_msg *msg)
{
	return (AUTH_OK);
}

int
svc_getcred(struct svc_req *rqst, struct ucred *cr, int *flavorp)
{
	int flavor, i;
	struct xucred *xcr;

	KASSERT(!crshared(cr), ("svc_getcred with shared cred"));

	flavor = rqst->rq_cred.oa_flavor;
	if (flavorp)
		*flavorp = flavor;

	switch (flavor) {
	case AUTH_UNIX:
		xcr = (struct xucred *) rqst->rq_clntcred;
		cr->cr_uid = cr->cr_ruid = cr->cr_svuid = xcr->cr_uid;
		cr->cr_ngroups = xcr->cr_ngroups;
		for (i = 0; i < xcr->cr_ngroups; i++)
			cr->cr_groups[i] = xcr->cr_groups[i];
		cr->cr_rgid = cr->cr_groups[0];
		return (TRUE);

	default:
		return (FALSE);
	}
}

