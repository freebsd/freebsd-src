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
 * svc_auth_unix.c
 * Handles UNIX flavor authentication parameters on the service side of rpc.
 * There are two svc auth implementations here: AUTH_UNIX and AUTH_SHORT.
 * _svcauth_unix does full blown unix style uid,gid+gids auth,
 * _svcauth_short uses a shorthand auth to index into a cache of longhand auths.
 * Note: the shorthand has been gutted for efficiency.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/ucred.h>

#include <rpc/rpc.h>

#include <rpc/rpc_com.h>

/*
 * Unix longhand authenticator
 */
enum auth_stat
_svcauth_unix(struct svc_req *rqst, struct rpc_msg *msg)
{
	enum auth_stat stat;
	XDR xdrs;
	int32_t *buf;
	struct xucred *xcr;
	uint32_t auth_len, time;

	xcr = rqst->rq_clntcred;
	auth_len = (u_int)msg->rm_call.cb_cred.oa_length;
	xdrmem_create(&xdrs, msg->rm_call.cb_cred.oa_base, auth_len,
	    XDR_DECODE);
	buf = XDR_INLINE(&xdrs, auth_len);
	if (buf != NULL) {
		/* 'time', 'str_len', UID, GID and 'supp_ngroups'. */
		const uint32_t min_len = 5 * BYTES_PER_XDR_UNIT;
		uint32_t str_len, supp_ngroups;

		if (auth_len < min_len)
			goto badcred;
		time = IXDR_GET_UINT32(buf);
		str_len = IXDR_GET_UINT32(buf);
		if (str_len > AUTH_SYS_MAX_HOSTNAME)
			goto badcred;
		str_len = RNDUP(str_len);
		/*
		 * Recheck message length now that we know the value of
		 * 'str_len' (and that it won't cause an overflow in additions
		 * below) to protect access to the credentials part.
		 */
		if (auth_len < min_len + str_len)
			goto badcred;
		buf += str_len / sizeof (int32_t);
		xcr->cr_uid = IXDR_GET_UINT32(buf);
		xcr->cr_gid = IXDR_GET_UINT32(buf);
		supp_ngroups = IXDR_GET_UINT32(buf);
		/*
		 * See the herald comment before a similar test at the end of
		 * xdr_authunix_parms() for why we strictly respect RFC 5531 and
		 * why we may have to drop the last supplementary group when
		 * there are AUTH_SYS_MAX_GROUPS of them.
		 */
		if (supp_ngroups > AUTH_SYS_MAX_GROUPS)
			goto badcred;
		/*
		 * Final message length check, as we now know how much we will
		 * read in total.
		 */
		if (auth_len < min_len + str_len +
		    supp_ngroups * BYTES_PER_XDR_UNIT)
			goto badcred;

		/*
		 * Note that 'xcr' is a 'struct xucred', which still has the
		 * historical layout where the effective GID is in cr_groups[0]
		 * and is accounted in 'cr_ngroups'.
		 */
		for (uint32_t i = 0; i < supp_ngroups; ++i) {
			if (i < XU_NGROUPS - 1)
				xcr->cr_sgroups[i] = IXDR_GET_INT32(buf);
			else
				buf++;
		}
		xcr->cr_ngroups = MIN(supp_ngroups + 1, XU_NGROUPS);
	} else if (!xdr_authunix_parms(&xdrs, &time, xcr))
		goto badcred;

	rqst->rq_verf = _null_auth;
	stat = AUTH_OK;
done:
	XDR_DESTROY(&xdrs);

	return (stat);

badcred:
	stat = AUTH_BADCRED;
	goto done;
}


/*
 * Shorthand unix authenticator
 * Looks up longhand in a cache.
 */
/*ARGSUSED*/
enum auth_stat
_svcauth_short(struct svc_req *rqst, struct rpc_msg *msg)
{
	return (AUTH_REJECTEDCRED);
}
