/*-
 * Copyright 1998 Juniper Networks, Inc.
 * All rights reserved.
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * Portions of this software was developed for the FreeBSD Project by
 * ThinkSec AS and NAI Labs, the Security Research Division of Network
 * Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yppasswd.h>
#endif

#include <login_cap.h>
#include <netdb.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include <pw_copy.h>
#include <pw_util.h>

#ifdef YP
#include <pw_yp.h>
#include "yppasswd_private.h"
#endif

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define	PAM_SM_SESSION
#define	PAM_SM_PASSWORD

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_mod_misc.h>

#define PASSWORD_HASH		"md5"
#define DEFAULT_WARN		(2L * 7L * 86400L)  /* Two weeks */
#define	SALTSIZE		32

static void makesalt(char []);

static char password_hash[] =		PASSWORD_HASH;
static char colon[] =			":";

enum {
	PAM_OPT_AUTH_AS_SELF	= PAM_OPT_STD_MAX,
	PAM_OPT_NULLOK,
	PAM_OPT_LOCAL_PASS,
	PAM_OPT_NIS_PASS
};

static struct opttab other_options[] = {
	{ "auth_as_self",	PAM_OPT_AUTH_AS_SELF },
	{ "nullok",		PAM_OPT_NULLOK },
	{ "local_pass",		PAM_OPT_LOCAL_PASS },
	{ "nis_pass",		PAM_OPT_NIS_PASS },
	{ NULL, 0 }
};

#ifdef YP
int pam_use_yp = 0;
int yp_errno = YP_TRUE;
#endif

char *tempname = NULL;
static int local_passwd(const char *user, const char *pass);
#ifdef YP
static int yp_passwd(const char *user, const char *pass);
#endif

/*
 * authentication management
 */
PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags __unused, int argc, const char **argv)
{
	login_cap_t *lc;
	struct options options;
	struct passwd *pwd;
	int retval;
	const char *pass, *user;
	char *encrypted, *password_prompt;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	if (pam_test_option(&options, PAM_OPT_AUTH_AS_SELF, NULL))
		pwd = getpwnam(getlogin());
	else {
		retval = pam_get_user(pamh, &user, NULL);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);
		pwd = getpwnam(user);
	}

	PAM_LOG("Got user: %s", user);

	lc = login_getclass(NULL);
	password_prompt = login_getcapstr(lc, "passwd_prompt",
	    password_prompt, NULL);
	login_close(lc);
	lc = NULL;

	if (pwd != NULL) {

		PAM_LOG("Doing real authentication");

		if (pwd->pw_passwd[0] == '\0'
		    && pam_test_option(&options, PAM_OPT_NULLOK, NULL)) {
			/*
			 * No password case. XXX Are we giving too much away
			 * by not prompting for a password?
			 */
			PAM_LOG("No password, and null password OK");
			PAM_RETURN(PAM_SUCCESS);
		}
		else {
			retval = pam_get_authtok(pamh, PAM_AUTHTOK,
			    &pass, password_prompt);
			if (retval != PAM_SUCCESS)
				PAM_RETURN(retval);
			PAM_LOG("Got password");
		}
		encrypted = crypt(pass, pwd->pw_passwd);
		if (pass[0] == '\0' && pwd->pw_passwd[0] != '\0')
			encrypted = colon;

		PAM_LOG("Encrypted password 1 is: %s", encrypted);
		PAM_LOG("Encrypted password 2 is: %s", pwd->pw_passwd);

		retval = strcmp(encrypted, pwd->pw_passwd) == 0 ?
		    PAM_SUCCESS : PAM_AUTH_ERR;
	}
	else {

		PAM_LOG("Doing dummy authentication");

		/*
		 * User unknown.
		 * Encrypt a dummy password so as to not give away too much.
		 */
		retval = pam_get_authtok(pamh,
		    PAM_AUTHTOK, &pass, password_prompt);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);
		PAM_LOG("Got password");
		crypt(pass, "xx");
		retval = PAM_AUTH_ERR;
	}

	/*
	 * The PAM infrastructure will obliterate the cleartext
	 * password before returning to the application.
	 */
	if (retval != PAM_SUCCESS)
		PAM_VERBOSE_ERROR("UNIX authentication refused");

	PAM_RETURN(retval);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh __unused, int flags __unused, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
}

/* 
 * account management
 */
PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags __unused, int argc, const char **argv)
{
	struct addrinfo hints, *res;
	struct options options;
	struct passwd *pwd;
	struct timeval tp;
	login_cap_t *lc;
	time_t warntime;
	int retval;
	const char *rhost, *tty, *user;
	char rhostip[MAXHOSTNAMELEN];

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	retval = pam_get_user(pamh, &user, NULL);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);

	if (user == NULL || (pwd = getpwnam(user)) == NULL)
		PAM_RETURN(PAM_SERVICE_ERR);

	PAM_LOG("Got user: %s", user);

	retval = pam_get_item(pamh, PAM_RHOST, (const void **)&rhost);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);

	retval = pam_get_item(pamh, PAM_TTY, (const void **)&tty);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);

	if (*pwd->pw_passwd == '\0' &&
	    (flags & PAM_DISALLOW_NULL_AUTHTOK) != 0)
		return (PAM_NEW_AUTHTOK_REQD);
	    
	lc = login_getpwclass(pwd);
	if (lc == NULL) {
		PAM_LOG("Unable to get login class for user %s", user);
		return (PAM_SERVICE_ERR);
	}

	PAM_LOG("Got login_cap");

	if (pwd->pw_change || pwd->pw_expire)
		gettimeofday(&tp, NULL);

	/*
	 * Check pw_expire before pw_change - no point in letting the
	 * user change the password on an expired account.
	 */
	
	if (pwd->pw_expire) {
		warntime = login_getcaptime(lc, "warnexpire",
		    DEFAULT_WARN, DEFAULT_WARN);
		if (tp.tv_sec >= pwd->pw_expire) {
			login_close(lc);
			PAM_RETURN(PAM_ACCT_EXPIRED);
		} else if (pwd->pw_expire - tp.tv_sec < warntime &&
		    (flags & PAM_SILENT) == 0) {
			pam_error(pamh, "Warning: your account expires on %s",
			    ctime(&pwd->pw_expire));
		}
	}

	retval = PAM_SUCCESS;
	if (pwd->pw_change) {
		warntime = login_getcaptime(lc, "warnpassword",
		    DEFAULT_WARN, DEFAULT_WARN);
		if (tp.tv_sec >= pwd->pw_change) {
			retval = PAM_NEW_AUTHTOK_REQD;
		} else if (pwd->pw_change - tp.tv_sec < warntime &&
		    (flags & PAM_SILENT) == 0) {
			pam_error(pamh, "Warning: your password expires on %s",
			    ctime(&pwd->pw_change));
		}
	}

	/*
	 * From here on, we must leave retval untouched (unless we
	 * know we're going to fail), because we need to remember
	 * whether we're supposed to return PAM_SUCCESS or
	 * PAM_NEW_AUTHTOK_REQD.
	 */

	if (rhost) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		if (getaddrinfo(rhost, NULL, &hints, &res) == 0) {
			getnameinfo(res->ai_addr, res->ai_addrlen,
			    rhostip, sizeof(rhostip), NULL, 0,
			    NI_NUMERICHOST|NI_WITHSCOPEID);
		}
		if (res != NULL)
			freeaddrinfo(res);
	}

	/*
	 * Check host / tty / time-of-day restrictions
	 */
	
	if (!auth_hostok(lc, rhost, rhostip) ||
	    !auth_ttyok(lc, tty) ||
	    !auth_timeok(lc, time(NULL)))
		retval = PAM_AUTH_ERR;
	
	login_close(lc);

	PAM_RETURN(retval);
}

/* 
 * session management
 *
 * logging only
 */
PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh __unused, int flags __unused, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh __unused, int flags __unused, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
}

/* 
 * password management
 *
 * standard Unix and NIS password changing
 */
PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;
	struct passwd *pwd;
	const char *user, *pass, *new_pass;
	char *encrypted, *usrdup;
	int retval, res;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	if (pam_test_option(&options, PAM_OPT_AUTH_AS_SELF, NULL))
		pwd = getpwnam(getlogin());
	else {
		retval = pam_get_user(pamh, &user, NULL);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);
		pwd = getpwnam(user);
	}

	PAM_LOG("Got user: %s", user);

	if (flags & PAM_PRELIM_CHECK) {

		PAM_LOG("PRELIM round; checking user password");

		if (pwd->pw_passwd[0] == '\0'
		    && pam_test_option(&options, PAM_OPT_NULLOK, NULL)) {
			/*
			 * No password case. XXX Are we giving too much away
			 * by not prompting for a password?
			 * XXX check PAM_DISALLOW_NULL_AUTHTOK
			 */
			PAM_LOG("Got password");
			PAM_RETURN(PAM_SUCCESS);
		}
		else {
			retval = pam_get_authtok(pamh,
			    PAM_OLDAUTHTOK, &pass, NULL);
			if (retval != PAM_SUCCESS)
				PAM_RETURN(retval);
			PAM_LOG("Got password");
		}
		encrypted = crypt(pass, pwd->pw_passwd);
		if (pass[0] == '\0' && pwd->pw_passwd[0] != '\0')
			encrypted = colon;

		if (strcmp(encrypted, pwd->pw_passwd) != 0) {
			pam_set_item(pamh, PAM_OLDAUTHTOK, NULL);
			PAM_RETURN(PAM_AUTH_ERR);
		}

		PAM_RETURN(PAM_SUCCESS);
	}
	else if (flags & PAM_UPDATE_AUTHTOK) {
		PAM_LOG("UPDATE round; checking user password");

		retval = pam_get_authtok(pamh, PAM_OLDAUTHTOK, &pass, NULL);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);

		PAM_LOG("Got old password");

		for (;;) {
			retval = pam_get_authtok(pamh,
			    PAM_AUTHTOK, &new_pass, NULL);
			if (retval != PAM_TRY_AGAIN)
				break;
			pam_error(pamh, "Mismatch; try again, EOF to quit.");
		}

		if (retval != PAM_SUCCESS) {
			PAM_VERBOSE_ERROR("Unable to get new password");
			PAM_RETURN(PAM_PERM_DENIED);
		}

		PAM_LOG("Got new password: %s", new_pass);

#ifdef YP
		/* If NIS is set in the passwd database, use it */
		if ((usrdup = strdup(user)) == NULL)
			PAM_RETURN(PAM_BUF_ERR);
		res = use_yp(usrdup, 0, 0);
		free(usrdup);
		if (res == USER_YP_ONLY) {
			if (!pam_test_option(&options, PAM_OPT_LOCAL_PASS,
			    NULL))
				retval = yp_passwd(user, new_pass);
			else {
				/* Reject 'local' flag if NIS is on and the user
				 * is not local
				 */
				retval = PAM_PERM_DENIED;
				PAM_LOG("Unknown local user: %s", user);
			}
		}
		else if (res == USER_LOCAL_ONLY) {
			if (!pam_test_option(&options, PAM_OPT_NIS_PASS, NULL))
				retval = local_passwd(user, new_pass);
			else {
				/* Reject 'nis' flag if user is only local */
				retval = PAM_PERM_DENIED;
				PAM_LOG("Unknown NIS user: %s", user);
			}
		}
		else if (res == USER_YP_AND_LOCAL) {
			if (pam_test_option(&options, PAM_OPT_NIS_PASS, NULL))
				retval = yp_passwd(user, new_pass);
			else
				retval = local_passwd(user, new_pass);
		}
		else
			retval = PAM_SERVICE_ERR; /* Bad juju */
#else
		retval = local_passwd(user, new_pass);
#endif
	}
	else {
		/* Very bad juju */
		retval = PAM_ABORT;
		PAM_LOG("Illegal 'flags'");
	}

	PAM_RETURN(retval);
}

/* Mostly stolen from passwd(1)'s local_passwd.c - markm */

static unsigned char itoa64[] =		/* 0 ... 63 => ascii - 64 */
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void
to64(char *s, long v, int n)
{
	while (--n >= 0) {
		*s++ = itoa64[v&0x3f];
		v >>= 6;
	}
}

static int
local_passwd(const char *user, const char *pass)
{
	login_cap_t * lc;
	struct passwd *pwd;
	int pfd, tfd;
	char *crypt_type, salt[SALTSIZE + 1];

	pwd = getpwnam(user);
	if (pwd == NULL)
		return(PAM_SERVICE_ERR); /* Really bad things */

#ifdef YP
	pwd = (struct passwd *)&local_password;
#endif
	pw_init();

	pwd->pw_change = 0;
	lc = login_getclass(NULL);
	crypt_type = login_getcapstr(lc, "passwd_format",
		password_hash, password_hash);
	if (login_setcryptfmt(lc, crypt_type, NULL) == NULL)
		syslog(LOG_ERR, "cannot set password cipher");
	login_close(lc);
	makesalt(salt);
	pwd->pw_passwd = crypt(pass, salt);

	pfd = pw_lock();
	tfd = pw_tmp();
	pw_copy(pfd, tfd, pwd, NULL);

	if (!pw_mkdb(user))
		pw_error((char *)NULL, 0, 1);

	return (PAM_SUCCESS);
}

#ifdef YP
/* Stolen from src/usr.bin/passwd/yp_passwd.c, carrying copyrights of:
 * Copyright (c) 1992/3 Theo de Raadt <deraadt@fsa.ca>
 * Copyright (c) 1994 Olaf Kirch <okir@monad.swb.de>
 * Copyright (c) 1995 Bill Paul <wpaul@ctr.columbia.edu>
 */
int
yp_passwd(const char *user __unused, const char *pass)
{
	struct yppasswd yppwd;
	struct master_yppasswd master_yppwd;
	struct passwd *pwd;
	struct rpc_err err;
	CLIENT *clnt;
	login_cap_t *lc;
	int    *status;
	uid_t uid;
	char   *master, sockname[] = YP_SOCKNAME, salt[SALTSIZE + 1];

	_use_yp = 1;

	uid = getuid();

	master = get_yp_master(1);
	if (master == NULL)
		return (PAM_SERVICE_ERR); /* Major disaster */

	/*
	 * It is presumed that by the time we get here, use_yp()
	 * has been called and that we have verified that the user
	 * actually exists. This being the case, the yp_password
	 * stucture has already been filled in for us.
	 */

	/* Use the correct password */
	pwd = (struct passwd *)&yp_password;

	pwd->pw_change = 0;

	/* Initialize password information */
	if (suser_override) {
		master_yppwd.newpw.pw_passwd = strdup(pwd->pw_passwd);
		master_yppwd.newpw.pw_name = strdup(pwd->pw_name);
		master_yppwd.newpw.pw_uid = pwd->pw_uid;
		master_yppwd.newpw.pw_gid = pwd->pw_gid;
		master_yppwd.newpw.pw_expire = pwd->pw_expire;
		master_yppwd.newpw.pw_change = pwd->pw_change;
		master_yppwd.newpw.pw_fields = pwd->pw_fields;
		master_yppwd.newpw.pw_gecos = strdup(pwd->pw_gecos);
		master_yppwd.newpw.pw_dir = strdup(pwd->pw_dir);
		master_yppwd.newpw.pw_shell = strdup(pwd->pw_shell);
		master_yppwd.newpw.pw_class = pwd->pw_class != NULL ?
					strdup(pwd->pw_class) : strdup("");
		master_yppwd.oldpass = strdup("");
		master_yppwd.domain = yp_domain;
	} else {
		yppwd.newpw.pw_passwd = strdup(pwd->pw_passwd);
		yppwd.newpw.pw_name = strdup(pwd->pw_name);
		yppwd.newpw.pw_uid = pwd->pw_uid;
		yppwd.newpw.pw_gid = pwd->pw_gid;
		yppwd.newpw.pw_gecos = strdup(pwd->pw_gecos);
		yppwd.newpw.pw_dir = strdup(pwd->pw_dir);
		yppwd.newpw.pw_shell = strdup(pwd->pw_shell);
		yppwd.oldpass = strdup("");
	}

	if (login_setcryptfmt(lc, "md5", NULL) == NULL)
		syslog(LOG_ERR, "cannot set password cipher");
	login_close(lc);

	makesalt(salt);
	if (suser_override)
		master_yppwd.newpw.pw_passwd = crypt(pass, salt);
	else
		yppwd.newpw.pw_passwd = crypt(pass, salt);

	if (suser_override) {
		if ((clnt = clnt_create(sockname, MASTER_YPPASSWDPROG,
		    MASTER_YPPASSWDVERS, "unix")) == NULL) {
			syslog(LOG_ERR,
			    "Cannot contact rpc.yppasswdd on host %s: %s",
			    master, clnt_spcreateerror(""));
			return (PAM_SERVICE_ERR);
		}
	}
	else {
		if ((clnt = clnt_create(master, YPPASSWDPROG,
		    YPPASSWDVERS, "udp")) == NULL) {
			syslog(LOG_ERR,
			    "Cannot contact rpc.yppasswdd on host %s: %s",
			    master, clnt_spcreateerror(""));
			return (PAM_SERVICE_ERR);
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
		status = yppasswdproc_update_master_1(&master_yppwd, clnt);
	else
		status = yppasswdproc_update_1(&yppwd, clnt);

	clnt_geterr(clnt, &err);

	auth_destroy(clnt->cl_auth);
	clnt_destroy(clnt);

	if (err.re_status != RPC_SUCCESS || status == NULL || *status)
		return (PAM_SERVICE_ERR);

	if (err.re_status || status == NULL || *status)
		return (PAM_SERVICE_ERR);
	return (PAM_SUCCESS);
}
#endif /* YP */

/* Salt suitable for traditional DES and MD5 */
void
makesalt(char salt[SALTSIZE])
{
	int i;

	/* These are not really random numbers, they are just
	 * numbers that change to thwart construction of a
	 * dictionary. This is exposed to the public.
	 */
	for (i = 0; i < SALTSIZE; i += 4)
		to64(&salt[i], arc4random(), 4);
	salt[SALTSIZE] = '\0';
}

PAM_MODULE_ENTRY("pam_unix");
