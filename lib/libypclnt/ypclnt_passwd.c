/*-
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>

#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yppasswd.h>

#include "ypclnt.h"

int
ypclnt_passwd(ypclnt_t *ypclnt, const struct passwd *pwd, const char *passwd)
{
	struct yppasswd yppwd;
	struct rpc_err rpcerr;
	CLIENT *clnt = NULL;
	int ret, *result;

	/* check that rpc.yppasswdd is running */
	if (getrpcport(ypclnt->server, YPPASSWDPROG,
		YPPASSWDPROC_UPDATE, IPPROTO_UDP) == 0) {
		ypclnt_error(ypclnt, __func__, "no rpc.yppasswdd on server");
		return (-1);
	}

	/* fill the yppasswd structure */
	memset(&yppwd, 0, sizeof yppwd);
	yppwd.newpw.pw_uid = pwd->pw_uid;
	yppwd.newpw.pw_gid = pwd->pw_gid;
	if ((yppwd.newpw.pw_name = strdup(pwd->pw_name)) == NULL ||
	    (yppwd.newpw.pw_passwd = strdup(pwd->pw_passwd)) == NULL ||
	    (yppwd.newpw.pw_gecos = strdup(pwd->pw_gecos)) == NULL ||
	    (yppwd.newpw.pw_dir = strdup(pwd->pw_dir)) == NULL ||
	    (yppwd.newpw.pw_shell = strdup(pwd->pw_shell)) == NULL ||
	    (yppwd.oldpass = strdup(passwd)) == NULL) {
		ypclnt_error(ypclnt, __func__, strerror(errno));
		ret = -1;
		goto done;
	}

	/* connect to rpc.yppasswdd */
	clnt = clnt_create(ypclnt->server, YPPASSWDPROG, YPPASSWDVERS, "udp");
	if (clnt == NULL) {
		ypclnt_error(ypclnt, __func__,
		    "failed to connect to rpc.yppasswdd: %s",
		    clnt_spcreateerror(ypclnt->server));
		ret = -1;
		goto done;
	}
	clnt->cl_auth = authunix_create_default();

	/* request the update */
	result = yppasswdproc_update_1(&yppwd, clnt);

	/* check for RPC errors */
	clnt_geterr(clnt, &rpcerr);
	if (rpcerr.re_status != RPC_SUCCESS) {
		ypclnt_error(ypclnt, __func__,
		    "NIS password update failed: %s",
		    clnt_sperror(clnt, ypclnt->server));
		ret = -1;
		goto done;
	}

	/* check the result of the update */
	if (result == NULL || *result != 0) {
		ypclnt_error(ypclnt, __func__,
		    "NIS password update failed");
		/* XXX how do we get more details? */
		ret = -1;
		goto done;
	}

	ypclnt_error(ypclnt, NULL, NULL);
	ret = 0;

 done:
	if (clnt != NULL) {
		auth_destroy(clnt->cl_auth);
		clnt_destroy(clnt);
	}
	free(yppwd.newpw.pw_name);
	free(yppwd.newpw.pw_passwd);
	free(yppwd.newpw.pw_gecos);
	free(yppwd.newpw.pw_dir);
	free(yppwd.newpw.pw_shell);
	if (yppwd.oldpass != NULL) {
		memset(yppwd.oldpass, 0, strlen(yppwd.oldpass));
		free(yppwd.oldpass);
	}
	return (ret);
}
