/*	$NetBSD: auth_unix.c,v 1.18 2000/07/06 03:03:30 christos Exp $	*/

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

#if defined(LIBC_SCCS) && !defined(lint)
static char *sccsid2 = "@(#)auth_unix.c 1.19 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)auth_unix.c	2.2 88/08/01 4.0 RPCSRC";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * auth_unix.c, Implements UNIX style authentication parameters.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * The system is very weak.  The client uses no encryption for it's
 * credentials and only sends null verifiers.  The server sends backs
 * null verifiers or optionally a verifier that suggests a new short hand
 * for the credentials.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/ucred.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>

#include <rpc/rpc_com.h>

/* auth_unix.c */
static void authunix_nextverf (AUTH *);
static bool_t authunix_marshal (AUTH *, XDR *);
static bool_t authunix_validate (AUTH *, struct opaque_auth *);
static bool_t authunix_refresh (AUTH *, void *);
static void authunix_destroy (AUTH *);
static void marshal_new_auth (AUTH *);

static struct auth_ops authunix_ops = {
	.ah_nextverf =		authunix_nextverf,
	.ah_marshal =		authunix_marshal,
	.ah_validate =		authunix_validate,
	.ah_refresh =		authunix_refresh,
	.ah_destroy =		authunix_destroy
};

/*
 * This struct is pointed to by the ah_private field of an auth_handle.
 */
struct audata {
	struct opaque_auth	au_origcred;	/* original credentials */
	struct opaque_auth	au_shcred;	/* short hand cred */
	u_long			au_shfaults;	/* short hand cache faults */
	char			au_marshed[MAX_AUTH_BYTES];
	u_int			au_mpos;	/* xdr pos at end of marshed */
};
#define	AUTH_PRIVATE(auth)	((struct audata *)auth->ah_private)

/*
 * Create a unix style authenticator.
 * Returns an auth handle with the given stuff in it.
 */
AUTH *
authunix_create(struct ucred *cred)
{
	struct xucred xcr;
	char mymem[MAX_AUTH_BYTES];
	XDR xdrs;
	AUTH *auth;
	struct audata *au;
	struct timeval now;
	uint32_t time;
	int len;

	/*
	 * Allocate and set up auth handle
	 */
	au = NULL;
	auth = mem_alloc(sizeof(*auth));
#ifndef _KERNEL
	if (auth == NULL) {
		printf("authunix_create: out of memory");
		goto cleanup_authunix_create;
	}
#endif
	au = mem_alloc(sizeof(*au));
#ifndef _KERNEL
	if (au == NULL) {
		printf("authunix_create: out of memory");
		goto cleanup_authunix_create;
	}
#endif
	auth->ah_ops = &authunix_ops;
	auth->ah_private = (caddr_t)au;
	auth->ah_verf = au->au_shcred = _null_auth;
	au->au_shfaults = 0;
	au->au_origcred.oa_base = NULL;

	getmicrotime(&now);
	time = now.tv_sec;

	/*
	 * Serialize the parameters into origcred
	 */
	xdrmem_create(&xdrs, mymem, MAX_AUTH_BYTES, XDR_ENCODE);
	cru2x(cred, &xcr);
	if (! xdr_authunix_parms(&xdrs, &time, &xcr)) 
		panic("authunix_create: failed to encode creds");
	au->au_origcred.oa_length = len = XDR_GETPOS(&xdrs);
	au->au_origcred.oa_flavor = AUTH_UNIX;
#ifdef _KERNEL
	au->au_origcred.oa_base = mem_alloc((u_int) len);
#else
	if ((au->au_origcred.oa_base = mem_alloc((u_int) len)) == NULL) {
		printf("authunix_create: out of memory");
		goto cleanup_authunix_create;
	}
#endif
	memcpy(au->au_origcred.oa_base, mymem, (size_t)len);

	/*
	 * set auth handle to reflect new cred.
	 */
	auth->ah_cred = au->au_origcred;
	marshal_new_auth(auth);
	return (auth);
#ifndef _KERNEL
 cleanup_authunix_create:
	if (auth)
		mem_free(auth, sizeof(*auth));
	if (au) {
		if (au->au_origcred.oa_base)
			mem_free(au->au_origcred.oa_base, (u_int)len);
		mem_free(au, sizeof(*au));
	}
	return (NULL);
#endif
}

/*
 * authunix operations
 */

/* ARGSUSED */
static void
authunix_nextverf(AUTH *auth)
{
	/* no action necessary */
}

static bool_t
authunix_marshal(AUTH *auth, XDR *xdrs)
{
	struct audata *au;

	au = AUTH_PRIVATE(auth);
	return (XDR_PUTBYTES(xdrs, au->au_marshed, au->au_mpos));
}

static bool_t
authunix_validate(AUTH *auth, struct opaque_auth *verf)
{
	struct audata *au;
	XDR xdrs;

	if (verf->oa_flavor == AUTH_SHORT) {
		au = AUTH_PRIVATE(auth);
		xdrmem_create(&xdrs, verf->oa_base, verf->oa_length,
		    XDR_DECODE);

		if (au->au_shcred.oa_base != NULL) {
			mem_free(au->au_shcred.oa_base,
			    au->au_shcred.oa_length);
			au->au_shcred.oa_base = NULL;
		}
		if (xdr_opaque_auth(&xdrs, &au->au_shcred)) {
			auth->ah_cred = au->au_shcred;
		} else {
			xdrs.x_op = XDR_FREE;
			(void)xdr_opaque_auth(&xdrs, &au->au_shcred);
			au->au_shcred.oa_base = NULL;
			auth->ah_cred = au->au_origcred;
		}
		marshal_new_auth(auth);
	}
	return (TRUE);
}

static bool_t
authunix_refresh(AUTH *auth, void *dummy)
{
	struct audata *au = AUTH_PRIVATE(auth);
	struct xucred xcr;
	uint32_t time;
	struct timeval now;
	XDR xdrs;
	int stat;

	if (auth->ah_cred.oa_base == au->au_origcred.oa_base) {
		/* there is no hope.  Punt */
		return (FALSE);
	}
	au->au_shfaults ++;

	/* first deserialize the creds back into a struct ucred */
	xdrmem_create(&xdrs, au->au_origcred.oa_base,
	    au->au_origcred.oa_length, XDR_DECODE);
	stat = xdr_authunix_parms(&xdrs, &time, &xcr);
	if (! stat)
		goto done;

	/* update the time and serialize in place */
	getmicrotime(&now);
	time = now.tv_sec;
	xdrs.x_op = XDR_ENCODE;
	XDR_SETPOS(&xdrs, 0);

	stat = xdr_authunix_parms(&xdrs, &time, &xcr);
	if (! stat)
		goto done;
	auth->ah_cred = au->au_origcred;
	marshal_new_auth(auth);
done:
	XDR_DESTROY(&xdrs);
	return (stat);
}

static void
authunix_destroy(AUTH *auth)
{
	struct audata *au;

	au = AUTH_PRIVATE(auth);
	mem_free(au->au_origcred.oa_base, au->au_origcred.oa_length);

	if (au->au_shcred.oa_base != NULL)
		mem_free(au->au_shcred.oa_base, au->au_shcred.oa_length);

	mem_free(auth->ah_private, sizeof(struct audata));

	if (auth->ah_verf.oa_base != NULL)
		mem_free(auth->ah_verf.oa_base, auth->ah_verf.oa_length);

	mem_free(auth, sizeof(*auth));
}

/*
 * Marshals (pre-serializes) an auth struct.
 * sets private data, au_marshed and au_mpos
 */
static void
marshal_new_auth(AUTH *auth)
{
	XDR	xdr_stream;
	XDR	*xdrs = &xdr_stream;
	struct audata *au;

	au = AUTH_PRIVATE(auth);
	xdrmem_create(xdrs, au->au_marshed, MAX_AUTH_BYTES, XDR_ENCODE);
	if ((! xdr_opaque_auth(xdrs, &(auth->ah_cred))) ||
	    (! xdr_opaque_auth(xdrs, &(auth->ah_verf))))
		printf("auth_none.c - Fatal marshalling problem");
	else
		au->au_mpos = XDR_GETPOS(xdrs);
	XDR_DESTROY(xdrs);
}
