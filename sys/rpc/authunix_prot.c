/*	$NetBSD: authunix_prot.c,v 1.12 2000/01/22 22:19:17 mycroft Exp $	*/

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
static char *sccsid2 = "@(#)authunix_prot.c 1.15 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)authunix_prot.c	2.1 88/07/29 4.0 RPCSRC";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * authunix_prot.c
 * XDR for UNIX style authentication parameters for RPC
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/ucred.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>

#include <rpc/rpc_com.h>

/* gids compose part of a credential; there may not be more than 16 of them */
#define NGRPS 16

/*
 * XDR for unix authentication parameters.
 */
bool_t
xdr_authunix_parms(XDR *xdrs, uint32_t *time, struct xucred *cred)
{
	uint32_t namelen;
	uint32_t ngroups, i;
	uint32_t junk;

	if (xdrs->x_op == XDR_ENCODE) {
		namelen = strlen(hostname);
	} else {
		namelen = 0;
	}
	junk = 0;

	if (!xdr_uint32_t(xdrs, time)
	    || !xdr_uint32_t(xdrs, &namelen))
		return (FALSE);

	/*
	 * Ignore the hostname on decode.
	 */
	if (xdrs->x_op == XDR_ENCODE) {
		if (!xdr_opaque(xdrs, hostname, namelen))
			return (FALSE);
	} else {
		xdr_setpos(xdrs, xdr_getpos(xdrs) + RNDUP(namelen));
	}

	if (!xdr_uint32_t(xdrs, &cred->cr_uid))
		return (FALSE);
	if (!xdr_uint32_t(xdrs, &cred->cr_groups[0]))
		return (FALSE);

	if (xdrs->x_op == XDR_ENCODE) {
		ngroups = cred->cr_ngroups - 1;
		if (ngroups > NGRPS)
			ngroups = NGRPS;
	}

	if (!xdr_uint32_t(xdrs, &ngroups))
		return (FALSE);
	for (i = 0; i < ngroups; i++) {
		if (i + 1 < NGROUPS) {
			if (!xdr_uint32_t(xdrs, &cred->cr_groups[i + 1]))
				return (FALSE);
		} else {
			if (!xdr_uint32_t(xdrs, &junk))
				return (FALSE);
		}
	}

	if (xdrs->x_op == XDR_DECODE) {
		if (ngroups + 1 > NGROUPS)
			cred->cr_ngroups = NGROUPS;
		else
			cred->cr_ngroups = ngroups + 1;
	}

	return (TRUE);
}
