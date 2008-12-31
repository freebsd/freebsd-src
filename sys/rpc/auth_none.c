/*	$NetBSD: auth_none.c,v 1.13 2000/01/22 22:19:17 mycroft Exp $	*/

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
static char *sccsid2 = "@(#)auth_none.c 1.19 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)auth_none.c	2.1 88/07/29 4.0 RPCSRC";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/rpc/auth_none.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * auth_none.c
 * Creates a client authentication handle for passing "null"
 * credentials and verifiers to remote systems.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>

#define MAX_MARSHAL_SIZE 20

/*
 * Authenticator operations routines
 */

static bool_t authnone_marshal (AUTH *, XDR *);
static void authnone_verf (AUTH *);
static bool_t authnone_validate (AUTH *, struct opaque_auth *);
static bool_t authnone_refresh (AUTH *, void *);
static void authnone_destroy (AUTH *);

static struct auth_ops authnone_ops = {
	.ah_nextverf =		authnone_verf,
	.ah_marshal =		authnone_marshal,
	.ah_validate =		authnone_validate,
	.ah_refresh =		authnone_refresh,
	.ah_destroy =		authnone_destroy
};

struct authnone_private {
	AUTH	no_client;
	char	mclient[MAX_MARSHAL_SIZE];
	u_int	mcnt;
};

static struct authnone_private authnone_private;

static void
authnone_init(void *dummy)
{
	struct authnone_private *ap = &authnone_private;
	XDR xdrs;

	ap->no_client.ah_cred = ap->no_client.ah_verf = _null_auth;
	ap->no_client.ah_ops = &authnone_ops;
	xdrmem_create(&xdrs, ap->mclient, MAX_MARSHAL_SIZE, XDR_ENCODE);
	xdr_opaque_auth(&xdrs, &ap->no_client.ah_cred);
	xdr_opaque_auth(&xdrs, &ap->no_client.ah_verf);
	ap->mcnt = XDR_GETPOS(&xdrs);
	XDR_DESTROY(&xdrs);
}
SYSINIT(authnone_init, SI_SUB_KMEM, SI_ORDER_ANY, authnone_init, NULL);

AUTH *
authnone_create()
{
	struct authnone_private *ap = &authnone_private;

	return (&ap->no_client);
}

/*ARGSUSED*/
static bool_t
authnone_marshal(AUTH *client, XDR *xdrs)
{
	struct authnone_private *ap = &authnone_private;

	KASSERT(xdrs != NULL, ("authnone_marshal: xdrs is null"));

	return (xdrs->x_ops->x_putbytes(xdrs, ap->mclient, ap->mcnt));
}

/* All these unused parameters are required to keep ANSI-C from grumbling */
/*ARGSUSED*/
static void
authnone_verf(AUTH *client)
{
}

/*ARGSUSED*/
static bool_t
authnone_validate(AUTH *client, struct opaque_auth *opaque)
{

	return (TRUE);
}

/*ARGSUSED*/
static bool_t
authnone_refresh(AUTH *client, void *dummy)
{

	return (FALSE);
}

/*ARGSUSED*/
static void
authnone_destroy(AUTH *client)
{
}
