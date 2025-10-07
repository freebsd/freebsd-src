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

/*
 * authunix_prot.c
 * XDR for UNIX style authentication parameters for RPC
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/libkern.h>
#include <sys/ucred.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>

#include <rpc/rpc_com.h>

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

	if (xdrs->x_op == XDR_FREE)
		/* This function does not allocate auxiliary memory. */
		return (TRUE);

	if (xdrs->x_op == XDR_ENCODE) {
		getcredhostname(NULL, hostbuf, sizeof(hostbuf));
		namelen = strlen(hostbuf);
		if (namelen > AUTH_SYS_MAX_HOSTNAME)
			namelen = AUTH_SYS_MAX_HOSTNAME;
	} else
		namelen = 0;

	if (!xdr_uint32_t(xdrs, time) || !xdr_uint32_t(xdrs, &namelen))
		return (FALSE);

	/*
	 * Ignore the hostname on decode.
	 */
	if (xdrs->x_op == XDR_ENCODE) {
		if (!xdr_opaque(xdrs, hostbuf, namelen))
			return (FALSE);
	} else {
		if (namelen > AUTH_SYS_MAX_HOSTNAME)
			return (FALSE);
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
		if (supp_ngroups > AUTH_SYS_MAX_GROUPS)
			/* With current values, this should never execute. */
			supp_ngroups = AUTH_SYS_MAX_GROUPS;
	}

	if (!xdr_uint32_t(xdrs, &supp_ngroups))
		return (FALSE);

	/*
	 * Because we cannot store more than XU_NGROUPS in total (16 at time of
	 * this writing), for now we choose to be strict with respect to RFC
	 * 5531's maximum number of supplementary groups (AUTH_SYS_MAX_GROUPS).
	 * That would also be an accidental DoS prevention measure if the
	 * request handling code didn't try to reassemble it in full without any
	 * size limits.  Although AUTH_SYS_MAX_GROUPS and XU_NGROUPS are equal,
	 * since the latter includes the "effective" GID, we cannot store the
	 * last group of a message with exactly AUTH_SYS_MAX_GROUPS
	 * supplementary groups.  We accept such messages so as not to violate
	 * the protocol, silently dropping the last group on the floor.
	 */

	if (xdrs->x_op != XDR_ENCODE && supp_ngroups > AUTH_SYS_MAX_GROUPS)
		return (FALSE);

	junk = 0;
	for (i = 0; i < supp_ngroups; ++i)
		if (!xdr_uint32_t(xdrs, i < XU_NGROUPS - 1 ?
		    &cred->cr_sgroups[i] : &junk))
			return (FALSE);

	if (xdrs->x_op != XDR_ENCODE)
		cred->cr_ngroups = MIN(supp_ngroups + 1, XU_NGROUPS);

	return (TRUE);
}
