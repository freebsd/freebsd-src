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
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
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
 * $FreeBSD$
 */

#ifdef YP
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
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
#include <rpcsvc/yp.h>
struct dom_binding {};
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yppasswd.h>
#include <pw_util.h>
#include "pw_yp.h"
#include "ypxfr_extern.h"
#include "yppasswd_private.h"

#define PERM_SECURE (S_IRUSR|S_IWUSR)
static HASHINFO openinfo = {
        4096,           /* bsize */
        32,             /* ffactor */
        256,            /* nelem */
        2048 * 1024,    /* cachesize */
        NULL,           /* hash */
        0,              /* lorder */
};

int force_old = 0;
int _use_yp = 0;
int suser_override = 0;
int yp_in_pw_file = 0;
char *yp_domain = NULL;
char *yp_server = NULL;

extern char *tempname;

/* Save the local and NIS password information */
struct passwd local_password;
struct passwd yp_password;

void copy_yp_pass(p, x, m)
char *p;
int x, m;
{
	register char *t, *s = p;
	static char *buf;

	yp_password.pw_fields = 0;

	buf = (char *)realloc(buf, m + 10);
	bzero(buf, m + 10);

	/* Turn all colons into NULLs */
	while (strchr(s, ':')) {
		s = (strchr(s, ':') + 1);
		*(s - 1)= '\0';
	}

	t = buf;
#define EXPAND(e)       e = t; while ((*t++ = *p++));
        EXPAND(yp_password.pw_name);
	yp_password.pw_fields |= _PWF_NAME;
        EXPAND(yp_password.pw_passwd);
	yp_password.pw_fields |= _PWF_PASSWD;
	yp_password.pw_uid = atoi(p);
        p += (strlen(p) + 1);
	yp_password.pw_fields |= _PWF_UID;
	yp_password.pw_gid = atoi(p);
        p += (strlen(p) + 1);
	yp_password.pw_fields |= _PWF_GID;
	if (x) {
		EXPAND(yp_password.pw_class);
		yp_password.pw_fields |= _PWF_CLASS;
		yp_password.pw_change = atol(p);
		p += (strlen(p) + 1);
		yp_password.pw_fields |= _PWF_CHANGE;
		yp_password.pw_expire = atol(p);
		p += (strlen(p) + 1);
		yp_password.pw_fields |= _PWF_EXPIRE;
	}
        EXPAND(yp_password.pw_gecos);
	yp_password.pw_fields |= _PWF_GECOS;
        EXPAND(yp_password.pw_dir);
	yp_password.pw_fields |= _PWF_DIR;
        EXPAND(yp_password.pw_shell);
	yp_password.pw_fields |= _PWF_SHELL;

	return;
}

void copy_local_pass(p,m)
char *p;
int m;
{
	register char *t;
	static char *buf;

	buf = (char *)realloc(buf, m + 10);
	bzero(buf, m + 10);

	t = buf;
        EXPAND(local_password.pw_name);
        EXPAND(local_password.pw_passwd);
        bcopy(p, (char *)&local_password.pw_uid, sizeof(int));
        p += sizeof(int);
        bcopy(p, (char *)&local_password.pw_gid, sizeof(int));
        p += sizeof(int);
       	bcopy(p, (char *)&local_password.pw_change, sizeof(time_t));
       	p += sizeof(time_t);
       	EXPAND(local_password.pw_class);
        EXPAND(local_password.pw_gecos);
        EXPAND(local_password.pw_dir);
        EXPAND(local_password.pw_shell);
        bcopy(p, (char *)&local_password.pw_expire, sizeof(time_t));
        p += sizeof(time_t);
        bcopy(p, (char *)&local_password.pw_fields, sizeof local_password.pw_fields);
        p += sizeof local_password.pw_fields;

	return;
}

/*
 * It is not mandatory that an NIS master server also be a client.
 * However, if the NIS master is not configured as a client, then the
 * domain name will not be set and ypbind will not be running, so we
 * will be unable to use the ypclnt routines inside libc. We therefore
 * need our own magic version of yp_match() which we can use in any
 * environment.
 */
static int my_yp_match(server, domain, map, key, keylen, result, resultlen)
	char *server;
	char *domain;
	char *map;
	char *key;
	unsigned long keylen;
	char **result;
	unsigned long *resultlen;
{
	ypreq_key ypkey;
	ypresp_val *ypval;
	CLIENT *clnt;
	static char buf[YPMAXRECORD + 2];

	bzero((char *)buf, sizeof(buf));

	/*
	 * Don't make this a fatal error. The inability to contact
	 * a server is, for our purposes, equivalent to not finding
	 * the record we were looking for. Letting use_yp() know
	 * that the lookup failed is sufficient.
	 */
	if ((clnt = clnt_create(server, YPPROG,YPVERS,"udp")) == NULL) {
		return(1);
#ifdef notdef
		warnx("failed to create UDP handle: %s",
					clnt_spcreateerror(server));
		pw_error(tempname, 0, 1);
#endif
	}

	ypkey.domain = domain;
	ypkey.map = map;
	ypkey.key.keydat_len = keylen;
	ypkey.key.keydat_val = key;

	if ((ypval = ypproc_match_2(&ypkey, clnt)) == NULL) {
		clnt_destroy(clnt);
		return(1);
#ifdef notdef
		warnx("%s",clnt_sperror(clnt,"YPPROC_MATCH failed"));
		pw_error(tempname, 0, 1);
#endif
	}

	clnt_destroy(clnt);

	if (ypval->stat != YP_TRUE) {
		xdr_free(xdr_ypresp_val, (char *)ypval);
		return(1);
#ifdef notdef
		int stat = ypval->stat;
		xdr_free(xdr_ypresp_val, (char *)ypval);
		if (stat == YP_NOMAP && strstr(map, "master.passwd"))
			return(1);
		if (stat == YP_NOKEY)
			return(1);
		warnx("ypmatch failed: %s", yperr_string(ypprot_err(stat)));
		pw_error(tempname, 0, 1);
#endif
	}


	strncpy((char *)&buf, ypval->val.valdat_val, ypval->val.valdat_len);

	*result = (char *)&buf;
	*resultlen = ypval->val.valdat_len;

	xdr_free(xdr_ypresp_val, (char *)ypval);

	return(0);
}

/*
 * Check if the user we're working with is local or in NIS.
 */
int use_yp (user, uid, which)
	char *user;
	uid_t uid;
	int which; /* 0 = use username, 1 = use uid */
{
	int user_local = 0, user_yp = 0, user_exists = 0;
	DB *dbp;
	DBT key,data;
	char bf[UT_NAMESIZE + 2];
	char *result;
	char *server;
	long resultlen;
	char ubuf[UT_NAMESIZE + 2];

	if (which) {
		snprintf(ubuf, sizeof(ubuf), "%lu", (unsigned long)uid);
		user = (char *)&ubuf;
	}

	/* Grope around for the user in the usual way */
	if (which) {
		if (getpwuid(uid) != NULL)
			user_exists = 1;
	} else {
		if (getpwnam(user) != NULL)
			user_exists = 1;
	}

	/* Now grope directly through the user database */
	if ((dbp = dbopen(_PATH_SMP_DB, O_RDONLY, PERM_SECURE,
			DB_HASH, &openinfo)) == NULL) {
			warn("error opening database: %s.", _PATH_MP_DB);
			pw_error(tempname, 0, 1);
	}

	/* Is NIS turned on */
	bf[0] = _PW_KEYYPENABLED;
	key.data = (u_char *)bf;
	key.size = 1;
	yp_in_pw_file = !(dbp->get)(dbp,&key,&data,0);
	if (_yp_check(NULL) || (yp_domain && yp_server)) {
		server = get_yp_master(0);

		/* Is the user in the NIS passwd map */
		if (!my_yp_match(server, yp_domain, which ? "passwd.byuid" :
					"passwd.byname", user, strlen(user),
		    &result, &resultlen)) {
			user_yp = user_exists = 1;
			*(char *)(result + resultlen) = '\0';
			copy_yp_pass(result, 0, resultlen);
		}
		/* Is the user in the NIS master.passwd map */
		if (user_yp && !my_yp_match(server, yp_domain, which ?
			"master.passwd.byuid" : "master.passwd.byname",
		    user, strlen(user),
		    &result, &resultlen)) {
			*(char *)(result + resultlen) = '\0';
			copy_yp_pass(result, 1, resultlen);
		}
	}

	/* Is the user in the local password database */

	bf[0] = which ? _PW_KEYBYUID : _PW_KEYBYNAME;
	if (which)
		bcopy((char *)&uid, bf + 1, sizeof(uid));
	else
		bcopy((char *)user, bf + 1, MIN(strlen(user), UT_NAMESIZE));
	key.data = (u_char *)bf;
	key.size = which ? sizeof(uid) + 1 : strlen(user) + 1;
	if (!(dbp->get)(dbp,&key,&data,0)) {
		user_local = 1;
		copy_local_pass(data.data, data.size);
	}

	(dbp->close)(dbp);

	if (user_local && user_yp && user_exists)
		return(USER_YP_AND_LOCAL);
	else if (!user_local && user_yp && user_exists)
		return(USER_YP_ONLY);
	else if (user_local && !user_yp && user_exists)
		return(USER_LOCAL_ONLY);
	else if (!user_exists)
		return(USER_UNKNOWN);

	return(-1);
}

/*
 * Find the name of the NIS master server for this domain
 * and make sure it's running yppasswdd.
 */
char *get_yp_master(getserver)
	int getserver;
{
	char *mastername;
	int rval, localport;
	struct stat st;

	/*
	 * Sometimes we are called just to probe for rpc.yppasswdd and
	 * set the suser_override flag. Just return NULL and leave
	 * suser_override at 0 if _use_yp doesn't indicate that NIS is
	 * in use and we weren't called from use_yp() itself.
	 * Without this check, we might try probing and fail with an NIS
	 * error in non-NIS environments.
	 */
	if ((_use_yp == USER_UNKNOWN || _use_yp == USER_LOCAL_ONLY) &&
								getserver)
		return(NULL);

	/* Get default NIS domain. */

	if (yp_domain == NULL && (rval = yp_get_default_domain(&yp_domain))) {
		warnx("can't get local NIS domain name: %s",yperr_string(rval));
		pw_error(tempname, 0, 1);
	}

	/* Get master server of passwd map. */

	if ((mastername = ypxfr_get_master(yp_domain, "passwd.byname",
				yp_server, yp_server ? 0 : 1)) == NULL) {
		warnx("can't get name of master NIS server");
		pw_error(tempname, 0, 1);
	}

	if (!getserver)
		return(mastername);

	/* Check if yppasswdd is out there. */

	if ((rval = getrpcport(mastername, YPPASSWDPROG, YPPASSWDPROC_UPDATE,
		IPPROTO_UDP)) == 0) {
		warnx("rpc.yppasswdd is not running on the NIS master server");
		pw_error(tempname, 0, 1);
	}

	/*
	 * Make sure it's on a reserved port.
	 * XXX Might break with yppasswdd servers running on Solaris 2.x.
	 */

	if (rval >= IPPORT_RESERVED) {
		warnx("rpc.yppasswdd server not running on reserved port");
		pw_error(tempname, 0, 1);
	}

	/* See if _we_ are the master server. */
	if (!force_old && !getuid() && (localport = getrpcport("localhost",
		YPPASSWDPROG, YPPASSWDPROC_UPDATE, IPPROTO_UDP)) != 0) {
		if (localport == rval) {
			suser_override = 1;
			mastername = "localhost";
		}
	}

	/* Everything checks out: return the name of the server. */

	return (mastername);
}

/*
 * Ask the user for his NIS password and submit the new information
 * to yppasswdd. Note that rpc.yppasswdd requires password authentication
 * and only allows changes to existing records rather than the addition
 * of new records. (To do actual updates we would need something like
 * secure RPC and ypupdated, which FreeBSD doesn't have yet.) The FreeBSD
 * rpc.yppasswdd has some special hooks to allow the superuser update
 * information without specifying a password, however this only works
 * for the superuser on the NIS master server.
 */
void yp_submit(pw)
	struct passwd *pw;
{
	struct yppasswd yppasswd;
	struct master_yppasswd master_yppasswd;
	struct netconfig *nconf;
	void *localhandle;
	CLIENT *clnt;
	char *master, *password;
	int *status = NULL;
	struct rpc_err err;

	nconf = NULL;
	_use_yp = 1;

	/* Get NIS master server name */

	master = get_yp_master(1);

	/* Populate the yppasswd structure that gets handed to yppasswdd. */

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
		master_yppasswd.oldpass = ""; /* not really needed */
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

	/* Get the user's password for authentication purposes. */

	printf ("Changing NIS information for %s on %s\n",
					pw->pw_name, master);

	if (pw->pw_passwd[0] && !suser_override) {
		password = getpass("Please enter password: ");
		if (strncmp(crypt(password,pw->pw_passwd),
				pw->pw_passwd,strlen(pw->pw_passwd))) {
			warnx("Password incorrect.");
			pw_error(tempname, 0, 1);
		}
		yppasswd.oldpass = password;	/* XXX */
	}

	if (suser_override) {
		/* Talk to server via AF_UNIX socket. */
		localhandle = setnetconfig();
		while ((nconf = getnetconfig(localhandle)) != NULL) {
			if (nconf->nc_protofmly != NULL &&
			    strcmp(nconf->nc_protofmly, NC_LOOPBACK) == 0)
				break;
		}
		if (nconf == NULL) {
			warnx("getnetconfig: %s", nc_sperror());
			pw_error(tempname, 0, 1);
		}

		clnt = clnt_tp_create(NULL, MASTER_YPPASSWDPROG,
		   MASTER_YPPASSWDVERS, nconf);
		if (clnt == NULL) {
			warnx("failed to contact rpc.yppasswdd: %s",
				clnt_spcreateerror(master));
			endnetconfig(localhandle);
			pw_error(tempname, 0, 1);
		}
		endnetconfig(localhandle);
	} else {
		/* Create a handle to yppasswdd. */

		if ((clnt = clnt_create(master, YPPASSWDPROG,
					YPPASSWDVERS, "udp")) == NULL) {
			warnx("failed to contact rpc.yppasswdd: %s",
				clnt_spcreateerror(master));
			pw_error(tempname, 0, 1);
		}
	}

	clnt->cl_auth = authunix_create_default();

	if (suser_override)
		status = yppasswdproc_update_master_1(&master_yppasswd, clnt);
	else
		status = yppasswdproc_update_1(&yppasswd, clnt);

	clnt_geterr(clnt, &err);

	auth_destroy(clnt->cl_auth);
	clnt_destroy(clnt);

	/* Call failed: signal the error. */

	if (err.re_status != RPC_SUCCESS || status == NULL || *status) {
		warnx("NIS update failed: %s", clnt_sperrno(err.re_status));
		pw_error(NULL, 0, 1);
	}

	/* Success. */

	if (suser_override)
		warnx("NIS information changed on host %s, domain %s",
			master, yp_domain);
	else
		warnx("NIS information changed on host %s", master);

	return;
}
#endif /* YP */
