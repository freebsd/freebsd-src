/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2009, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 * - Neither the name of Sun Microsystems, Inc. nor the names of its 
 *   contributors may be used to endorse or promote products derived 
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
/*
 * auth_none.c
 * Creates a client authentication handle for passing "null"
 * credentials and verifiers to remote systems.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

/*
 * Modified from auth_none.c to expect a reply verifier of "STARTTLS"
 * for the RPC-over-TLS STARTTLS command.
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
#include <rpc/clnt.h>
#include <rpc/rpcsec_tls.h>

#define MAX_MARSHAL_SIZE 20

/*
 * Authenticator operations routines
 */

static bool_t authtls_marshal (AUTH *, uint32_t, XDR *, struct mbuf *);
static void authtls_verf (AUTH *);
static bool_t authtls_validate (AUTH *, uint32_t, struct opaque_auth *,
    struct mbuf **);
static bool_t authtls_refresh (AUTH *, void *);
static void authtls_destroy (AUTH *);

static const struct auth_ops authtls_ops = {
	.ah_nextverf =		authtls_verf,
	.ah_marshal =		authtls_marshal,
	.ah_validate =		authtls_validate,
	.ah_refresh =		authtls_refresh,
	.ah_destroy =		authtls_destroy,
};

struct authtls_private {
	AUTH	no_client;
	char	mclient[MAX_MARSHAL_SIZE];
	u_int	mcnt;
};

static struct authtls_private authtls_private;
static struct opaque_auth _tls_null_auth;

static void
authtls_init(void *dummy)
{
	struct authtls_private *ap = &authtls_private;
	XDR xdrs;

	_tls_null_auth.oa_flavor = AUTH_TLS;
	_tls_null_auth.oa_base = NULL;
	_tls_null_auth.oa_length = 0;
	ap->no_client.ah_cred = _tls_null_auth;
	ap->no_client.ah_verf = _null_auth;
	ap->no_client.ah_ops = &authtls_ops;
	xdrmem_create(&xdrs, ap->mclient, MAX_MARSHAL_SIZE, XDR_ENCODE);
	xdr_opaque_auth(&xdrs, &ap->no_client.ah_cred);
	xdr_opaque_auth(&xdrs, &ap->no_client.ah_verf);
	ap->mcnt = XDR_GETPOS(&xdrs);
	XDR_DESTROY(&xdrs);
}
SYSINIT(authtls_init, SI_SUB_KMEM, SI_ORDER_ANY, authtls_init, NULL);

AUTH *
authtls_create(void)
{
	struct authtls_private *ap = &authtls_private;

	return (&ap->no_client);
}

/*ARGSUSED*/
static bool_t
authtls_marshal(AUTH *client, uint32_t xid, XDR *xdrs, struct mbuf *args)
{
	struct authtls_private *ap = &authtls_private;

	KASSERT(xdrs != NULL, ("authtls_marshal: xdrs is null"));

	if (!XDR_PUTBYTES(xdrs, ap->mclient, ap->mcnt))
		return (FALSE);

	xdrmbuf_append(xdrs, args);

	return (TRUE);
}

/* All these unused parameters are required to keep ANSI-C from grumbling */
/*ARGSUSED*/
static void
authtls_verf(AUTH *client)
{
}

/*ARGSUSED*/
static bool_t
authtls_validate(AUTH *client, uint32_t xid, struct opaque_auth *opaque,
    struct mbuf **mrepp)
{
	size_t strsiz;

	strsiz = strlen(RPCTLS_START_STRING);
	/* The verifier must be the string RPCTLS_START_STRING. */
	if (opaque != NULL &&
	    (opaque->oa_length != strsiz || memcmp(opaque->oa_base,
	     RPCTLS_START_STRING, strsiz) != 0))
		return (FALSE);
	return (TRUE);
}

/*ARGSUSED*/
static bool_t
authtls_refresh(AUTH *client, void *dummy)
{

	return (FALSE);
}

/*ARGSUSED*/
static void
authtls_destroy(AUTH *client)
{
}
