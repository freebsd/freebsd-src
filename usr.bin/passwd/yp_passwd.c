/*
 * Copyright (c) 1992/3 Theo de Raadt <deraadt@fsa.ca>
 * Copyright (c) 1994 Olaf Kirch <okir@monad.swb.de>
 * Copyright (c) 1995 Bill Paul <wpaul@ctr.columbia.edu>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifdef YP
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yppasswd.h>
#include <pw_yp.h>
#include <err.h>
#include "yppasswd_private.h"

extern char *getnewpasswd __P(( struct passwd * , int ));

int
yp_passwd(char *user)
{
	struct yppasswd yppasswd;
	struct master_yppasswd master_yppasswd;
	struct netconfig *nconf;
	void *localhandle;
	struct passwd *pw;
	CLIENT *clnt;
	struct rpc_err err;
	char   *master;
	int    *status = NULL;
	uid_t	uid;

	nconf = NULL;
	_use_yp = 1;

	uid = getuid();

	if ((master = get_yp_master(1)) == NULL) {
		warnx("failed to find NIS master server");
		return(1);
	}

	/*
	 * It is presumed that by the time we get here, use_yp()
	 * has been called and that we have verified that the user
	 * actually exists. This being the case, the yp_password
	 * stucture has already been filled in for us.
	 */

	/* Use the correct password */
	pw = (struct passwd *)&yp_password;

	if (pw->pw_uid != uid && uid != 0) {
		warnx("only the super-user may change account information \
for other users");
		return(1);
	}

	pw->pw_change = 0;

	/* Initialize password information */
	if (suser_override) {
		master_yppasswd.newpw.pw_passwd = strdup(pw->pw_passwd);
		master_yppasswd.newpw.pw_name = strdup(pw->pw_name);
		master_yppasswd.newpw.pw_uid = pw->pw_uid;
		master_yppasswd.newpw.pw_gid = pw->pw_gid;
		master_yppasswd.newpw.pw_expire = pw->pw_expire;
		master_yppasswd.newpw.pw_change = pw->pw_change;
		master_yppasswd.newpw.pw_fields = pw->pw_fields;
		master_yppasswd.newpw.pw_gecos = strdup(pw->pw_gecos);
		master_yppasswd.newpw.pw_dir = strdup(pw->pw_dir);
		master_yppasswd.newpw.pw_shell = strdup(pw->pw_shell);
		master_yppasswd.newpw.pw_class = pw->pw_class != NULL ?
					strdup(pw->pw_class) : "";
		master_yppasswd.oldpass = "";
		master_yppasswd.domain = yp_domain;
	} else {
		yppasswd.newpw.pw_passwd = strdup(pw->pw_passwd);
		yppasswd.newpw.pw_name = strdup(pw->pw_name);
		yppasswd.newpw.pw_uid = pw->pw_uid;
		yppasswd.newpw.pw_gid = pw->pw_gid;
		yppasswd.newpw.pw_gecos = strdup(pw->pw_gecos);
		yppasswd.newpw.pw_dir = strdup(pw->pw_dir);
		yppasswd.newpw.pw_shell = strdup(pw->pw_shell);
		yppasswd.oldpass = "";
	}

	if (suser_override)
		printf("Changing NIS password for %s on %s in domain %s.\n",
			pw->pw_name, master, yp_domain);
	else
		printf("Changing NIS password for %s on %s.\n",
							pw->pw_name, master);

	/* Get old password */

	if(pw->pw_passwd[0] && !suser_override) {
		yppasswd.oldpass = strdup(getpass("Old Password: "));
		if (strcmp(crypt(yppasswd.oldpass, pw->pw_passwd),
							pw->pw_passwd)) {
			errx(1, "sorry");
		}

	}

	if (suser_override) {
		if ((master_yppasswd.newpw.pw_passwd = getnewpasswd(pw, 1)) == NULL)
			return(1);
	} else {
		if ((yppasswd.newpw.pw_passwd = getnewpasswd(pw, 1)) == NULL)
			return(1);
	}

	if (suser_override) {
		localhandle = setnetconfig();
		while ((nconf = getnetconfig(localhandle)) != NULL) {
			if (nconf->nc_protofmly != NULL &&
			    strcmp(nconf->nc_protofmly, NC_LOOPBACK) == 0)
				break;
		}
		if (nconf == NULL) {
			warnx("getnetconfig: %s", nc_sperror());
			return(1);
		}
		if ((clnt = clnt_tp_create(NULL, MASTER_YPPASSWDPROG,
		    MASTER_YPPASSWDVERS, nconf)) == NULL) {
			warnx("failed to contact rpc.yppasswdd on host %s: %s",
				master, clnt_spcreateerror(""));
			endnetconfig(localhandle);
			return(1);
		}
		endnetconfig(localhandle);
	} else {
		if ((clnt = clnt_create(master, YPPASSWDPROG,
				YPPASSWDVERS, "udp")) == NULL) {
			warnx("failed to contact rpc.yppasswdd on host %s: %s",
				master, clnt_spcreateerror(""));
			return(1);
		}
	}
	/*
	 * The yppasswd.x file said `unix authentication required',
	 * so I added it. This is the only reason it is in here.
	 * My yppasswdd doesn't use it, but maybe some others out there
	 * do. 					--okir
	 */
	clnt->cl_auth = authunix_create_default();

	if (suser_override)
		status = yppasswdproc_update_master_1(&master_yppasswd, clnt);
	else
		status = yppasswdproc_update_1(&yppasswd, clnt);

	clnt_geterr(clnt, &err);

	auth_destroy(clnt->cl_auth);
	clnt_destroy(clnt);

	if (err.re_status != RPC_SUCCESS || status == NULL || *status) {
		errx(1, "failed to change NIS password: %s", 
			clnt_sperrno(err.re_status));
	}

	printf("\nNIS password has%s been changed on %s.\n",
		(err.re_status != RPC_SUCCESS || status == NULL || *status) ?
		" not" : "", master);

	return ((err.re_status || status == NULL || *status));
}
#endif /* YP */
