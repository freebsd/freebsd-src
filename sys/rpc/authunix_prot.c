/*	$NetBSD: authunix_prot.c,v 1.12 2000/01/22 22:19:17 mycroft Exp $	*/

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

#if defined(LIBC_SCCS) && !defined(lint)
static char *sccsid2 = "@(#)authunix_prot.c 1.15 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)authunix_prot.c	2.1 88/07/29 4.0 RPCSRC";
#endif
#include <sys/cdefs.h>
/*
 * authunix_prot.c
 * XDR for UNIX style authentication parameters for RPC
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/jail.h>
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
	uint32_t supp_ngroups, i;
	uint32_t junk;
	char hostbuf[MAXHOSTNAMELEN];

	if (xdrs->x_op == XDR_ENCODE) {
		/*
		 * Restrict name length to 255 according to RFC 1057.
		 */
		getcredhostname(NULL, hostbuf, sizeof(hostbuf));
		namelen = strlen(hostbuf);
		if (namelen > 255)
			namelen = 255;
	} else {
		namelen = 0;
	}

	if (!xdr_uint32_t(xdrs, time)
	    || !xdr_uint32_t(xdrs, &namelen))
		return (FALSE);

	/*
	 * Ignore the hostname on decode.
	 */
	if (xdrs->x_op == XDR_ENCODE) {
		if (!xdr_opaque(xdrs, hostbuf, namelen))
			return (FALSE);
	} else {
		xdr_setpos(xdrs, xdr_getpos(xdrs) + RNDUP(namelen));
	}

	if (!xdr_uint32_t(xdrs, &cred->cr_uid))
		return (FALSE);

	/*
	 * Safety check: The protocol needs at least one group (access to
	 * 'cr_gid', decrementation of 'cr_ngroups' below).
	 */
	if (xdrs->x_op == XDR_ENCODE && cred->cr_ngroups == 0)
		return (FALSE);
	if (!xdr_uint32_t(xdrs, &cred->cr_gid))
		return (FALSE);

	if (xdrs->x_op == XDR_ENCODE) {
		/*
		 * Note that this is a 'struct xucred', which still has the
		 * historical layout where the effective GID is in cr_groups[0]
		 * and is accounted in 'cr_ngroups'.  We substract 1 to obtain
		 * the number of "supplementary" groups, passed in the AUTH_SYS
		 * credentials variable-length array called gids[] in RFC 5531.
		 */
		MPASS(cred->cr_ngroups <= XU_NGROUPS);
		supp_ngroups = cred->cr_ngroups - 1;
		if (supp_ngroups > NGRPS)
			supp_ngroups = NGRPS;
	}

	if (!xdr_uint32_t(xdrs, &supp_ngroups))
		return (FALSE);

	junk = 0;
	for (i = 0; i < supp_ngroups; ++i)
		if (!xdr_uint32_t(xdrs, i < XU_NGROUPS - 1 ?
		    &cred->cr_groups[i + 1] : &junk))
			return (FALSE);

	if (xdrs->x_op != XDR_ENCODE)
		cred->cr_ngroups = MIN(supp_ngroups + 1, XU_NGROUPS);

	return (TRUE);
}
