/*
 * Copyright (c) 1995
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * NIS interface routines for chpass
 * 
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Center for Telecommunications Research
 * Columbia University, New York City
 *
 *	$Id$
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
#include <err.h>
#include <unistd.h>
#include <db.h>
#include <fcntl.h>
#include <utmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <limits.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yppasswd.h>
#include <pw_util.h>
#include "pw_yp.h"

#define PERM_SECURE (S_IRUSR|S_IWUSR)
HASHINFO openinfo = {
        4096,           /* bsize */
        32,             /* ffactor */
        256,            /* nelem */
        2048 * 1024,    /* cachesize */
        NULL,           /* hash */
        0,              /* lorder */
};

int _use_yp = 0;

/*
 * Check if the user we're working with is local or in NIS.
 */
int use_yp (user)
char *user;
{
	int yp_enabled = 0, user_not_local = 0, exists = 0;
	DB *dbp;
	DBT key,data;
	char bf[UT_NAMESIZE + 2];

	if ((dbp = dbopen(_PATH_MP_DB, O_RDONLY, PERM_SECURE,
			DB_HASH, &openinfo)) == NULL)
			errx(1, "error opening database: %s.", _PATH_MP_DB);
	bf[0] = _PW_KEYYPENABLED;
	key.data = (u_char *)bf;
	key.size = 1;
	if (!(dbp->get)(dbp,&key,&data,0))
		yp_enabled = 1;

	bf[0] = _PW_KEYBYNAME;
	bcopy((char *)user, bf + 1, MIN(strlen(user), UT_NAMESIZE));
	key.data = (u_char *)bf;
	key.size = strlen(user) + 1;
	if ((dbp->get)(dbp,&key,&data,0))
		user_not_local = 1;
	
	(dbp->close)(dbp);

	if (getpwnam(user) != NULL)
		exists = 1;

	if (yp_enabled && user_not_local && exists)
		return(1);
	else
		return(0);
}

/*
 * Find the name of the NIS master server for this domain
 * and make sure it's running yppasswdd.
 */
static char *get_yp_master(void)
{
	char *domain, *mastername;
	int rval;

	/* Get default NIS domain. */

	if ((rval = yp_get_default_domain(&domain))) {
		warnx("can't get local NIS domain name: %s",yperr_string(rval));
		pw_error(NULL, 0, 1);
	}

	/* Get master server of passwd map. */

	if ((rval = yp_master(domain, "passwd.byname", &mastername))) {
		warnx("can't get master NIS server: %s", yperr_string(rval));
		pw_error(NULL, 0, 1);
	}

	/* Check if yppasswdd is out there. */

	if ((rval = getrpcport(mastername, YPPASSWDPROG, YPPASSWDPROC_UPDATE,
		IPPROTO_UDP)) == 0) {
		warnx("yppasswdd not running on NIS master server");
		pw_error(NULL, 0, 1);
	}

	/*
	 * Make sure it's on a reserved port.
	 * XXX Might break with yppasswdd servers running on Solaris 2.x.
	 */

	if (rval >= IPPORT_RESERVED) {
		warnx("yppasswdd server not running on reserved port");
		pw_error(NULL, 0, 1);
	}

	/* Everything checks out: return the name of the server. */

	return (mastername);
}
/*
 * Ask the user for his NIS password and submit the new information
 * to yppasswdd. Note that yppasswdd requires password authentication
 * and only allows changes to existing records rather than the addition
 * of new records. (To do actual updates we would need something like
 * secure RPC and ypupdated, which FreeBSD doesn't have yet.) This means
 * that the superuser cannot use chpass(1) to add new users records to
 * the NIS password database.
 */
void yp_submit(pw)
struct passwd *pw;
{
	struct yppasswd yppasswd;
	CLIENT *clnt;
	char *master, *password, *encpass;
	int rval, status = 0;
	struct timeval	tv;

	/* Populate the yppasswd structure that gets handed to yppasswdd. */
	/*
	 * XXX This is done first to work around what looks like a very
	 * strange memory corruption bug: the text fields pointed to
	 * by the members of the 'pw' structure appear to be clobbered
	 * after get_yp_master() returns (in particular, it happens
	 * during getrpcport()). I don't know exactly where the problem
	 * lies: I traced it all the way to gethostbyname(), then gave
	 * up.
	 */
	yppasswd.newpw.pw_passwd = strdup(pw->pw_passwd);
	yppasswd.newpw.pw_name = strdup(pw->pw_name);
	yppasswd.newpw.pw_uid = pw->pw_uid;
	yppasswd.newpw.pw_gid = pw->pw_gid;
	yppasswd.newpw.pw_gecos = strdup(pw->pw_gecos);
	yppasswd.newpw.pw_dir = strdup(pw->pw_dir);
	yppasswd.newpw.pw_shell = strdup(pw->pw_shell);

	/* Get NIS master server name */

	master = get_yp_master();

	/* Get the user's password for authentication purposes. */

	printf ("Changing NIS information for %s on %s\n",
					yppasswd.newpw.pw_name, master);
	encpass = (getpwnam(yppasswd.newpw.pw_name))->pw_passwd;
	password = getpass("Please enter password: ");
	if (strncmp(crypt(password, encpass), encpass, strlen(encpass))) {
		warnx("Password incorrect.");
		pw_error(NULL, 0, 1);
	}

	yppasswd.oldpass = password;	/* XXX */

	/* Create a handle to yppasswdd. */

	clnt = clnt_create(master, YPPASSWDPROG, YPPASSWDVERS, "udp");
	clnt->cl_auth = authunix_create_default();

	/* Set a timeout and make the RPC call. */

	tv.tv_sec = 20;
	tv.tv_usec = 0;
	rval = clnt_call(clnt, YPPASSWDPROC_UPDATE, xdr_yppasswd,
		(char *)&yppasswd, xdr_int, (char *)&status, &tv);

	/* Call failed: signal the error. */

	if (rval) {
		warnx("NIS update failed: %s", clnt_sperrno(rval));
		pw_error(NULL, 0, 1);
	}

	/* Success. */

	auth_destroy(clnt->cl_auth);
	clnt_destroy(clnt);
	warnx("NIS information changed on host %s", master);

	return;
}
#endif /* YP */
