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
static char *sccsid2 = "@(#)svc_auth_unix.c 1.28 88/02/08 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)svc_auth_unix.c	2.3 88/08/01 4.0 RPCSRC";
#endif
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
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
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
	uint32_t time;
	struct xucred *xcr;
	u_int auth_len;
	size_t str_len, supp_ngroups;
	u_int i;

	xcr = rqst->rq_clntcred;
	auth_len = (u_int)msg->rm_call.cb_cred.oa_length;
	xdrmem_create(&xdrs, msg->rm_call.cb_cred.oa_base, auth_len,
	    XDR_DECODE);
	buf = XDR_INLINE(&xdrs, auth_len);
	if (buf != NULL) {
		time = IXDR_GET_UINT32(buf);
		str_len = (size_t)IXDR_GET_UINT32(buf);
		if (str_len > AUTH_SYS_MAX_HOSTNAME) {
			stat = AUTH_BADCRED;
			goto done;
		}
		str_len = RNDUP(str_len);
		buf += str_len / sizeof (int32_t);
		xcr->cr_uid = IXDR_GET_UINT32(buf);
		xcr->cr_gid = IXDR_GET_UINT32(buf);
		supp_ngroups = (size_t)IXDR_GET_UINT32(buf);
		if (supp_ngroups > AUTH_SYS_MAX_GROUPS) {
			stat = AUTH_BADCRED;
			goto done;
		}
		for (i = 0; i < supp_ngroups; i++) {
			/*
			 * Note that this is a `struct xucred`, which maintains
			 * its historical layout of preserving the egid in
			 * cr_ngroups and cr_groups[0] == egid.
			 */
			if (i + 1 < XU_NGROUPS)
				xcr->cr_groups[i + 1] = IXDR_GET_INT32(buf);
			else
				buf++;
		}
		if (supp_ngroups + 1 > XU_NGROUPS)
			xcr->cr_ngroups = XU_NGROUPS;
		else
			xcr->cr_ngroups = supp_ngroups + 1;

		/*
		 * five is the smallest unix credentials structure -
		 * timestamp, hostname len (0), uid, gid, and gids len (0).
		 */
		if ((5 + supp_ngroups) * BYTES_PER_XDR_UNIT + str_len > auth_len) {
			(void) printf("bad auth_len gid %ld str %ld auth %u\n",
			    (long)supp_ngroups, (long)str_len, auth_len);
			stat = AUTH_BADCRED;
			goto done;
		}
	} else if (! xdr_authunix_parms(&xdrs, &time, xcr)) {
		stat = AUTH_BADCRED;
		goto done;
	}

	rqst->rq_verf = _null_auth;
	stat = AUTH_OK;
done:
	XDR_DESTROY(&xdrs);

	return (stat);
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
