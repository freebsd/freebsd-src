/*
 * Main coding by Elliot Lee <sopwith@redhat.com>, Red Hat Software. 
 * Copyright (C) 1996.
 * Copyright (c) Jan Rêkorajski, 1999.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <security/_pam_aconf.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <syslog.h>
#include <shadow.h>
#include <time.h>		/* for time() */
#include <fcntl.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#ifdef USE_CRACKLIB
#include <crack.h>
#endif

#include <security/_pam_macros.h>

/* indicate the following groups are defined */

#define PAM_SM_PASSWORD

#include <security/pam_modules.h>

#ifndef LINUX_PAM
#include <security/pam_appl.h>
#endif				/* LINUX_PAM */

#include "yppasswd.h"
#include "md5.h"
#include "support.h"

#if !((__GLIBC__ == 2) && (__GLIBC_MINOR__ >= 1))
extern int getrpcport(const char *host, unsigned long prognum,
		      unsigned long versnum, unsigned int proto);
#endif				/* GNU libc 2.1 */

/*
 * PAM framework looks for these entry-points to pass control to the
 * password changing module.
 */

#ifdef NEED_LCKPWDF
#include "./lckpwdf.-c"
#endif

extern char *bigcrypt(const char *key, const char *salt);

/*
   How it works:
   Gets in username (has to be done) from the calling program
   Does authentication of user (only if we are not running as root)
   Gets new password/checks for sanity
   Sets it.
 */

/* passwd/salt conversion macros */

#define ascii_to_bin(c) ((c)>='a'?(c-59):(c)>='A'?((c)-53):(c)-'.')
#define bin_to_ascii(c) ((c)>=38?((c)-38+'a'):(c)>=12?((c)-12+'A'):(c)+'.')

/* data tokens */

#define _UNIX_OLD_AUTHTOK	"-UN*X-OLD-PASS"
#define _UNIX_NEW_AUTHTOK	"-UN*X-NEW-PASS"

#define MAX_PASSWD_TRIES	3
#define PW_TMPFILE		"/etc/npasswd"
#define SH_TMPFILE		"/etc/nshadow"
#define CRACKLIB_DICTS		"/usr/share/dict/cracklib_dict"
#define OPW_TMPFILE		"/etc/security/nopasswd"
#define OLD_PASSWORDS_FILE	"/etc/security/opasswd"

/*
 * i64c - convert an integer to a radix 64 character
 */
static int i64c(int i)
{
	if (i < 0)
		return ('.');
	else if (i > 63)
		return ('z');
	if (i == 0)
		return ('.');
	if (i == 1)
		return ('/');
	if (i >= 2 && i <= 11)
		return ('0' - 2 + i);
	if (i >= 12 && i <= 37)
		return ('A' - 12 + i);
	if (i >= 38 && i <= 63)
		return ('a' - 38 + i);
	return ('\0');
}

static char *crypt_md5_wrapper(const char *pass_new)
{
	/*
	 * Code lifted from Marek Michalkiewicz's shadow suite. (CG)
	 * removed use of static variables (AGM)
	 */

	struct timeval tv;
	MD5_CTX ctx;
	unsigned char result[16];
	char *cp = (char *) result;
	unsigned char tmp[16];
	int i;
	char *x, *e = NULL;

	GoodMD5Init(&ctx);
	gettimeofday(&tv, (struct timezone *) 0);
	GoodMD5Update(&ctx, (void *) &tv, sizeof tv);
	i = getpid();
	GoodMD5Update(&ctx, (void *) &i, sizeof i);
	i = clock();
	GoodMD5Update(&ctx, (void *) &i, sizeof i);
	GoodMD5Update(&ctx, result, sizeof result);
	GoodMD5Final(tmp, &ctx);
	strcpy(cp, "$1$");	/* magic for the MD5 */
	cp += strlen(cp);
	for (i = 0; i < 8; i++)
		*cp++ = i64c(tmp[i] & 077);
	*cp = '\0';

	/* no longer need cleartext */
	e = Goodcrypt_md5(pass_new, (const char *) result);
	x = x_strdup(e);	/* put e in malloc()ed memory */
	_pam_overwrite(e);	/* clean up */

	return x;
}

static char *getNISserver(pam_handle_t *pamh)
{
	char *master;
	char *domainname;
	int port, err;

	if ((err = yp_get_default_domain(&domainname)) != 0) {
		_log_err(LOG_WARNING, pamh, "can't get local yp domain: %s\n",
			 yperr_string(err));
		return NULL;
	}
	if ((err = yp_master(domainname, "passwd.byname", &master)) != 0) {
		_log_err(LOG_WARNING, pamh, "can't find the master ypserver: %s\n",
			 yperr_string(err));
		return NULL;
	}
	port = getrpcport(master, YPPASSWDPROG, YPPASSWDPROC_UPDATE, IPPROTO_UDP);
	if (port == 0) {
		_log_err(LOG_WARNING, pamh,
		         "yppasswdd not running on NIS master host\n");
		return NULL;
	}
	if (port >= IPPORT_RESERVED) {
		_log_err(LOG_WARNING, pamh,
		         "yppasswd daemon running on illegal port.\n");
		return NULL;
	}
	return master;
}

static int check_old_password(const char *forwho, const char *newpass)
{
	static char buf[16384];
	char *s_luser, *s_uid, *s_npas, *s_pas;
	int retval = PAM_SUCCESS;
	FILE *opwfile;

	opwfile = fopen(OLD_PASSWORDS_FILE, "r");
	if (opwfile == NULL)
		return PAM_AUTHTOK_ERR;

	while (fgets(buf, 16380, opwfile)) {
		if (!strncmp(buf, forwho, strlen(forwho))) {
			buf[strlen(buf) - 1] = '\0';
			s_luser = strtok(buf, ":,");
			s_uid = strtok(NULL, ":,");
			s_npas = strtok(NULL, ":,");
			s_pas = strtok(NULL, ":,");
			while (s_pas != NULL) {
				if (!strcmp(Goodcrypt_md5(newpass, s_pas), s_pas)) {
					retval = PAM_AUTHTOK_ERR;
					break;
				}
				s_pas = strtok(NULL, ":,");
			}
			break;
		}
	}
	fclose(opwfile);

	return retval;
}

static int save_old_password(const char *forwho, const char *oldpass, int howmany)
{
	static char buf[16384];
	static char nbuf[16384];
	char *s_luser, *s_uid, *s_npas, *s_pas, *pass;
	int retval = 0, npas;
	FILE *pwfile, *opwfile;
	int err = 0;
	int oldmask;
	int found = 0;
	struct passwd *pwd = NULL;

	if (howmany < 0)
		return retval;

	if (oldpass == NULL)
		return retval;

	oldmask = umask(077);
	pwfile = fopen(OPW_TMPFILE, "w");
	umask(oldmask);
	opwfile = fopen(OLD_PASSWORDS_FILE, "r");
	if (pwfile == NULL || opwfile == NULL)
		return PAM_AUTHTOK_ERR;
	chown(OPW_TMPFILE, 0, 0);
	chmod(OPW_TMPFILE, 0600);

	while (fgets(buf, 16380, opwfile)) {
		if (!strncmp(buf, forwho, strlen(forwho))) {
			buf[strlen(buf) - 1] = '\0';
			s_luser = strtok(buf, ":");
			s_uid = strtok(NULL, ":");
			s_npas = strtok(NULL, ":");
			s_pas = strtok(NULL, ":");
			npas = strtol(s_npas, NULL, 10) + 1;
			while (npas > howmany) {
				s_pas = strpbrk(s_pas, ",");
				if (s_pas != NULL)
					s_pas++;
				npas--;
			}
			pass = crypt_md5_wrapper(oldpass);
			if (s_pas == NULL)
				sprintf(nbuf, "%s:%s:%d:%s\n", s_luser, s_uid, npas, pass);
			else
				sprintf(nbuf, "%s:%s:%d:%s,%s\n", s_luser, s_uid, npas, s_pas, pass);
			if (fputs(nbuf, pwfile) < 0) {
				retval = PAM_AUTHTOK_ERR;
				err = 1;
				break;
			}
			found = 1;
		} else if (fputs(buf, pwfile) < 0) {
			retval = PAM_AUTHTOK_ERR;
			err = 1;
			break;
		}
	}
	fclose(opwfile);
	if (!found) {
		pwd = getpwnam(forwho);
		if (pwd == NULL) {
			retval = PAM_AUTHTOK_ERR;
			err = 1;
		} else {
			pass = crypt_md5_wrapper(oldpass);
			sprintf(nbuf, "%s:%d:1:%s\n", forwho, pwd->pw_uid, pass);
			if (fputs(nbuf, pwfile) < 0) {
				retval = PAM_AUTHTOK_ERR;
				err = 1;
			}
		}
	}
	if (fclose(pwfile)) {
		fprintf(stderr, "error writing entries to old passwords file: %s\n",
			strerror(errno));
		retval = PAM_AUTHTOK_ERR;
		err = 1;
	}
	if (!err)
		rename(OPW_TMPFILE, OLD_PASSWORDS_FILE);
	else
		unlink(OPW_TMPFILE);

	return retval;
}

static int _update_passwd(const char *forwho, const char *towhat)
{
	struct passwd *tmpent = NULL;
	FILE *pwfile, *opwfile;
	int retval = 0;
	int err = 0;
	int oldmask;

	oldmask = umask(077);
	pwfile = fopen(PW_TMPFILE, "w");
	umask(oldmask);
	opwfile = fopen("/etc/passwd", "r");
	if (pwfile == NULL || opwfile == NULL)
		return PAM_AUTHTOK_ERR;
	chown(PW_TMPFILE, 0, 0);
	chmod(PW_TMPFILE, 0644);
	tmpent = fgetpwent(opwfile);
	while (tmpent) {
		if (!strcmp(tmpent->pw_name, forwho)) {
			tmpent->pw_passwd = towhat;
		}
		if (putpwent(tmpent, pwfile)) {
			fprintf(stderr, "error writing entry to password file: %s\n",
				strerror(errno));
			err = 1;
			retval = PAM_AUTHTOK_ERR;
			break;
		}
		tmpent = fgetpwent(opwfile);
	}
	fclose(opwfile);

	if (fclose(pwfile)) {
		fprintf(stderr, "error writing entries to password file: %s\n",
			strerror(errno));
		retval = PAM_AUTHTOK_ERR;
		err = 1;
	}
	if (!err)
		rename(PW_TMPFILE, "/etc/passwd");
	else
		unlink(PW_TMPFILE);

	return retval;
}

static int _update_shadow(const char *forwho, char *towhat)
{
	struct spwd *spwdent = NULL, *stmpent = NULL;
	FILE *pwfile, *opwfile;
	int retval = 0;
	int err = 0;
	int oldmask;

	spwdent = getspnam(forwho);
	if (spwdent == NULL)
		return PAM_USER_UNKNOWN;
	oldmask = umask(077);
	pwfile = fopen(SH_TMPFILE, "w");
	umask(oldmask);
	opwfile = fopen("/etc/shadow", "r");
	if (pwfile == NULL || opwfile == NULL)
		return PAM_AUTHTOK_ERR;
	chown(SH_TMPFILE, 0, 0);
	chmod(SH_TMPFILE, 0600);
	stmpent = fgetspent(opwfile);
	while (stmpent) {
		if (!strcmp(stmpent->sp_namp, forwho)) {
			stmpent->sp_pwdp = towhat;
			stmpent->sp_lstchg = time(NULL) / (60 * 60 * 24);

			D(("Set password %s for %s", stmpent->sp_pwdp, forwho));
		}
		if (putspent(stmpent, pwfile)) {
			fprintf(stderr, "error writing entry to shadow file: %s\n",
				strerror(errno));
			err = 1;
			retval = PAM_AUTHTOK_ERR;
			break;
		}
		stmpent = fgetspent(opwfile);
	}
	fclose(opwfile);

	if (fclose(pwfile)) {
		fprintf(stderr, "error writing entries to shadow file: %s\n",
			strerror(errno));
		retval = PAM_AUTHTOK_ERR;
		err = 1;
	}
	if (!err)
		rename(SH_TMPFILE, "/etc/shadow");
	else
		unlink(SH_TMPFILE);

	return retval;
}

static int _do_setpass(pam_handle_t* pamh, const char *forwho, char *fromwhat,
		       char *towhat, unsigned int ctrl, int remember)
{
	struct passwd *pwd = NULL;
	int retval = 0;

	D(("called"));

	setpwent();
	pwd = getpwnam(forwho);
	endpwent();

	if (pwd == NULL)
		return PAM_AUTHTOK_ERR;

	if (on(UNIX_NIS, ctrl)) {
		struct timeval timeout;
		struct yppasswd yppwd;
		CLIENT *clnt;
		char *master;
		int status;
		int err = 0;

		/* Make RPC call to NIS server */
		if ((master = getNISserver(pamh)) == NULL)
			return PAM_TRY_AGAIN;

		/* Initialize password information */
		yppwd.newpw.pw_passwd = pwd->pw_passwd;
		yppwd.newpw.pw_name = pwd->pw_name;
		yppwd.newpw.pw_uid = pwd->pw_uid;
		yppwd.newpw.pw_gid = pwd->pw_gid;
		yppwd.newpw.pw_gecos = pwd->pw_gecos;
		yppwd.newpw.pw_dir = pwd->pw_dir;
		yppwd.newpw.pw_shell = pwd->pw_shell;
		yppwd.oldpass = fromwhat;
		yppwd.newpw.pw_passwd = towhat;

		D(("Set password %s for %s", yppwd.newpw.pw_passwd, forwho));

		/* The yppasswd.x file said `unix authentication required',
		 * so I added it. This is the only reason it is in here.
		 * My yppasswdd doesn't use it, but maybe some others out there
		 * do.                                        --okir
		 */
		clnt = clnt_create(master, YPPASSWDPROG, YPPASSWDVERS, "udp");
		clnt->cl_auth = authunix_create_default();
		memset((char *) &status, '\0', sizeof(status));
		timeout.tv_sec = 25;
		timeout.tv_usec = 0;
		err = clnt_call(clnt, YPPASSWDPROC_UPDATE,
				(xdrproc_t) xdr_yppasswd, (char *) &yppwd,
				(xdrproc_t) xdr_int, (char *) &status,
				timeout);

		if (err) {
			clnt_perrno(err);
			retval = PAM_TRY_AGAIN;
		} else if (status) {
			fprintf(stderr, "Error while changing NIS password.\n");
			retval = PAM_TRY_AGAIN;
		}
		printf("\nThe password has%s been changed on %s.\n",
		       (err || status) ? " not" : "", master);

		auth_destroy(clnt->cl_auth);
		clnt_destroy(clnt);
		if ((err || status) != 0) {
			retval = PAM_TRY_AGAIN;
		}
#ifdef DEBUG
		sleep(5);
#endif
		return retval;
	}
	/* first, save old password */
	if (save_old_password(forwho, fromwhat, remember)) {
		return PAM_AUTHTOK_ERR;
	}
	if (on(UNIX_SHADOW, ctrl) || (strcmp(pwd->pw_passwd, "x") == 0)) {
		retval = _update_shadow(forwho, towhat);
		if (retval == PAM_SUCCESS)
			retval = _update_passwd(forwho, "x");
	} else {
		retval = _update_passwd(forwho, towhat);
	}

	return retval;
}

static int _unix_verify_shadow(const char *user, unsigned int ctrl)
{
	struct passwd *pwd = NULL;	/* Password and shadow password */
	struct spwd *spwdent = NULL;	/* file entries for the user */
	time_t curdays;
	int retval = PAM_SUCCESS;

	/* UNIX passwords area */
	setpwent();
	pwd = getpwnam(user);	/* Get password file entry... */
	endpwent();
	if (pwd == NULL)
		return PAM_AUTHINFO_UNAVAIL;	/* We don't need to do the rest... */

	if (strcmp(pwd->pw_passwd, "x") == 0) {
		/* ...and shadow password file entry for this user, if shadowing
		   is enabled */
		setspent();
		spwdent = getspnam(user);
		endspent();

		if (spwdent == NULL)
			return PAM_AUTHINFO_UNAVAIL;
	} else {
		if (strcmp(pwd->pw_passwd,"*NP*") == 0) { /* NIS+ */                 
			uid_t save_uid;

			save_uid = geteuid();
			seteuid (pwd->pw_uid);
			spwdent = getspnam( user );
			seteuid (save_uid);

			if (spwdent == NULL)
				return PAM_AUTHINFO_UNAVAIL;
		} else
			spwdent = NULL;
	}

	if (spwdent != NULL) {
		/* We have the user's information, now let's check if their account
		   has expired (60 * 60 * 24 = number of seconds in a day) */

		if (off(UNIX__IAMROOT, ctrl)) {
			/* Get the current number of days since 1970 */
			curdays = time(NULL) / (60 * 60 * 24);
			if ((curdays < (spwdent->sp_lstchg + spwdent->sp_min))
			    && (spwdent->sp_min != -1))
				retval = PAM_AUTHTOK_ERR;
			else if ((curdays > (spwdent->sp_lstchg + spwdent->sp_max + spwdent->sp_inact))
				 && (spwdent->sp_max != -1) && (spwdent->sp_inact != -1)
				 && (spwdent->sp_lstchg != 0))
				/*
				 * Their password change has been put off too long,
				 */
				retval = PAM_ACCT_EXPIRED;
			else if ((curdays > spwdent->sp_expire) && (spwdent->sp_expire != -1)
				 && (spwdent->sp_lstchg != 0))
				/*
				 * OR their account has just plain expired
				 */
				retval = PAM_ACCT_EXPIRED;
		}
	}
	return retval;
}

static int _pam_unix_approve_pass(pam_handle_t * pamh
				  ,unsigned int ctrl
				  ,const char *pass_old
				  ,const char *pass_new)
{
	const char *user;
	const char *remark = NULL;
	int retval = PAM_SUCCESS;

	D(("&new=%p, &old=%p", pass_old, pass_new));
	D(("new=[%s]", pass_new));
	D(("old=[%s]", pass_old));

	if (pass_new == NULL || (pass_old && !strcmp(pass_old, pass_new))) {
		if (on(UNIX_DEBUG, ctrl)) {
			_log_err(LOG_DEBUG, pamh, "bad authentication token");
		}
		_make_remark(pamh, ctrl, PAM_ERROR_MSG, pass_new == NULL ?
			  "No password supplied" : "Password unchanged");
		return PAM_AUTHTOK_ERR;
	}
	/*
	 * if one wanted to hardwire authentication token strength
	 * checking this would be the place - AGM
	 */

	retval = pam_get_item(pamh, PAM_USER, (const void **) &user);
	if (retval != PAM_SUCCESS) {
		if (on(UNIX_DEBUG, ctrl)) {
			_log_err(LOG_ERR, pamh, "Can not get username");
			return PAM_AUTHTOK_ERR;
		}
	}
	if (off(UNIX__IAMROOT, ctrl)) {
#ifdef USE_CRACKLIB
		remark = FascistCheck(pass_new, CRACKLIB_DICTS);
		D(("called cracklib [%s]", remark));
#else
		if (strlen(pass_new) < 6)
			remark = "You must choose a longer password";
		D(("lenth check [%s]", remark));
#endif
		if (on(UNIX_REMEMBER_PASSWD, ctrl))
			if ((retval = check_old_password(user, pass_new)) != PAM_SUCCESS)
				remark = "Password has been already used. Choose another.";
	}
	if (remark) {
		_make_remark(pamh, ctrl, PAM_ERROR_MSG, remark);
		retval = PAM_AUTHTOK_ERR;
	}
	return retval;
}


PAM_EXTERN int pam_sm_chauthtok(pam_handle_t * pamh, int flags,
				int argc, const char **argv)
{
	unsigned int ctrl, lctrl;
	int retval, i;
	int remember = -1;

	/* <DO NOT free() THESE> */
	const char *user;
	char *pass_old, *pass_new;
	/* </DO NOT free() THESE> */

	D(("called."));

#ifdef USE_LCKPWDF
	/* our current locking system requires that we lock the
	   entire password database.  This avoids both livelock
	   and deadlock. */
	/* These values for the number of attempts and the sleep time
	   are, of course, completely arbitrary.
	   My reading of the PAM docs is that, once pam_chauthtok() has been
	   called with PAM_UPDATE_AUTHTOK, we are obliged to take any
	   reasonable steps to make sure the token is updated; so retrying
	   for 1/10 sec. isn't overdoing it.
	   The other possibility is to call lckpwdf() on the first
	   pam_chauthtok() pass, and hold the lock until released in the
	   second pass--but is this guaranteed to work? -SRL */
	i=0;
	while((retval = lckpwdf()) != 0 && i < 100) {
		usleep(1000);
	}
	if(retval != 0) {
		return PAM_AUTHTOK_LOCK_BUSY;
	}
#endif
	ctrl = _set_ctrl(pamh, flags, &remember, argc, argv);

	/*
	 * First get the name of a user
	 */
	retval = pam_get_user(pamh, &user, "Username: ");
	if (retval == PAM_SUCCESS) {
		/*
		 * Various libraries at various times have had bugs related to
		 * '+' or '-' as the first character of a user name. Don't take
		 * any chances here. Require that the username starts with an
		 * alphanumeric character.
		 */
		if (user == NULL || !isalnum(*user)) {
			_log_err(LOG_ERR, pamh, "bad username [%s]", user);
#ifdef USE_LCKPWDF
			ulckpwdf();
#endif
			return PAM_USER_UNKNOWN;
		}
		if (retval == PAM_SUCCESS && on(UNIX_DEBUG, ctrl))
			_log_err(LOG_DEBUG, pamh, "username [%s] obtained",
			         user);
	} else {
		if (on(UNIX_DEBUG, ctrl))
			_log_err(LOG_DEBUG, pamh,
			         "password - could not identify user");
#ifdef USE_LCKPWDF
		ulckpwdf();
#endif
		return retval;
	}

	D(("Got username of %s", user));

	/*
	 * This is not an AUTH module!
	 */
	if (on(UNIX__NONULL, ctrl))
		set(UNIX__NULLOK, ctrl);

	if (on(UNIX__PRELIM, ctrl)) {
		/*
		 * obtain and verify the current password (OLDAUTHTOK) for
		 * the user.
		 */
		char *Announce;

		D(("prelim check"));

		if (_unix_blankpasswd(ctrl, user)) {
#ifdef USE_LCKPWDF
			ulckpwdf();
#endif
			return PAM_SUCCESS;
		} else if (off(UNIX__IAMROOT, ctrl)) {

			/* instruct user what is happening */
#define greeting "Changing password for "
			Announce = (char *) malloc(sizeof(greeting) + strlen(user));
			if (Announce == NULL) {
				_log_err(LOG_CRIT, pamh,
				         "password - out of memory");
#ifdef USE_LCKPWDF
				ulckpwdf();
#endif
				return PAM_BUF_ERR;
			}
			(void) strcpy(Announce, greeting);
			(void) strcpy(Announce + sizeof(greeting) - 1, user);
#undef greeting

			lctrl = ctrl;
			set(UNIX__OLD_PASSWD, lctrl);
			retval = _unix_read_password(pamh, lctrl
						     ,Announce
					     ,"(current) UNIX password: "
						     ,NULL
						     ,_UNIX_OLD_AUTHTOK
					     ,(const char **) &pass_old);
			free(Announce);

			if (retval != PAM_SUCCESS) {
				_log_err(LOG_NOTICE, pamh
				 ,"password - (old) token not obtained");
#ifdef USE_LCKPWDF
				ulckpwdf();
#endif
				return retval;
			}
			/* verify that this is the password for this user */

			retval = _unix_verify_password(pamh, user, pass_old, ctrl);
		} else {
			D(("process run by root so do nothing this time around"));
			pass_old = NULL;
			retval = PAM_SUCCESS;	/* root doesn't have too */
		}

		if (retval != PAM_SUCCESS) {
			D(("Authentication failed"));
			pass_old = NULL;
#ifdef USE_LCKPWDF
			ulckpwdf();
#endif
			return retval;
		}
		retval = pam_set_item(pamh, PAM_OLDAUTHTOK, (const void *) pass_old);
		pass_old = NULL;
		if (retval != PAM_SUCCESS) {
			_log_err(LOG_CRIT, pamh,
			         "failed to set PAM_OLDAUTHTOK");
		}
		retval = _unix_verify_shadow(user, ctrl);
		if (retval == PAM_AUTHTOK_ERR) {
			if (off(UNIX__IAMROOT, ctrl))
				_make_remark(pamh, ctrl, PAM_ERROR_MSG,
					    "You must wait longer to change your password");
			else
				retval = PAM_SUCCESS;
		}
	} else if (on(UNIX__UPDATE, ctrl)) {
		/*
		 * tpass is used below to store the _pam_md() return; it
		 * should be _pam_delete()'d.
		 */

		char *tpass = NULL;
		int retry = 0;

		/*
		 * obtain the proposed password
		 */

		D(("do update"));

		/*
		 * get the old token back. NULL was ok only if root [at this
		 * point we assume that this has already been enforced on a
		 * previous call to this function].
		 */

		if (off(UNIX_NOT_SET_PASS, ctrl)) {
			retval = pam_get_item(pamh, PAM_OLDAUTHTOK
					      ,(const void **) &pass_old);
		} else {
			retval = pam_get_data(pamh, _UNIX_OLD_AUTHTOK
					      ,(const void **) &pass_old);
			if (retval == PAM_NO_MODULE_DATA) {
				retval = PAM_SUCCESS;
				pass_old = NULL;
			}
		}
		D(("pass_old [%s]", pass_old));

		if (retval != PAM_SUCCESS) {
			_log_err(LOG_NOTICE, pamh, "user not authenticated");
#ifdef USE_LCKPWDF
			ulckpwdf();
#endif
			return retval;
		}
		retval = _unix_verify_shadow(user, ctrl);
		if (retval != PAM_SUCCESS) {
			_log_err(LOG_NOTICE, pamh, "user not authenticated 2");
#ifdef USE_LCKPWDF
			ulckpwdf();
#endif
			return retval;
		}
		D(("get new password now"));

		lctrl = ctrl;

		if (on(UNIX_USE_AUTHTOK, lctrl)) {
			set(UNIX_USE_FIRST_PASS, lctrl);
		}
		retry = 0;
		retval = PAM_AUTHTOK_ERR;
		while ((retval != PAM_SUCCESS) && (retry++ < MAX_PASSWD_TRIES)) {
			/*
			 * use_authtok is to force the use of a previously entered
			 * password -- needed for pluggable password strength checking
			 */

			retval = _unix_read_password(pamh, lctrl
						     ,NULL
					     ,"Enter new UNIX password: "
					    ,"Retype new UNIX password: "
						     ,_UNIX_NEW_AUTHTOK
					     ,(const char **) &pass_new);

			if (retval != PAM_SUCCESS) {
				if (on(UNIX_DEBUG, ctrl)) {
					_log_err(LOG_ALERT, pamh
						 ,"password - new password not obtained");
				}
				pass_old = NULL;	/* tidy up */
#ifdef USE_LCKPWDF
				ulckpwdf();
#endif
				return retval;
			}
			D(("returned to _unix_chauthtok"));

			/*
			 * At this point we know who the user is and what they
			 * propose as their new password. Verify that the new
			 * password is acceptable.
			 */

			if (pass_new[0] == '\0') {	/* "\0" password = NULL */
				pass_new = NULL;
			}
			retval = _pam_unix_approve_pass(pamh, ctrl, pass_old, pass_new);
		}

		if (retval != PAM_SUCCESS) {
			_log_err(LOG_NOTICE, pamh,
			         "new password not acceptable");
			_pam_overwrite(pass_new);
			_pam_overwrite(pass_old);
			pass_new = pass_old = NULL;	/* tidy up */
#ifdef USE_LCKPWDF
			ulckpwdf();
#endif
			return retval;
		}
		/*
		 * By reaching here we have approved the passwords and must now
		 * rebuild the password database file.
		 */

		/*
		 * First we encrypt the new password.
		 */

		if (on(UNIX_MD5_PASS, ctrl)) {
			tpass = crypt_md5_wrapper(pass_new);
		} else {
			/*
			 * Salt manipulation is stolen from Rick Faith's passwd
			 * program.  Sorry Rick :) -- alex
			 */

			time_t tm;
			char salt[3];

			time(&tm);
			salt[0] = bin_to_ascii(tm & 0x3f);
			salt[1] = bin_to_ascii((tm >> 6) & 0x3f);
			salt[2] = '\0';

			if (off(UNIX_BIGCRYPT, ctrl) && strlen(pass_new) > 8) {
				/* 
				 * to avoid using the _extensions_ of the bigcrypt()
				 * function we truncate the newly entered password
				 */
				char *temp = malloc(9);
				char *e;

				if (temp == NULL) {
					_log_err(LOG_CRIT, pamh,
					         "out of memory for password");
					_pam_overwrite(pass_new);
					_pam_overwrite(pass_old);
					pass_new = pass_old = NULL;	/* tidy up */
#ifdef USE_LCKPWDF
					ulckpwdf();
#endif
					return PAM_BUF_ERR;
				}
				/* copy first 8 bytes of password */
				strncpy(temp, pass_new, 8);
				temp[8] = '\0';

				/* no longer need cleartext */
				e = bigcrypt(temp, salt);
				tpass = x_strdup(e);

				_pam_overwrite(e);
				_pam_delete(temp);	/* tidy up */
			} else {
				char *e;

				/* no longer need cleartext */
				e = bigcrypt(pass_new, salt);
				tpass = x_strdup(e);

				_pam_overwrite(e);
			}
		}

		D(("password processed"));

		/* update the password database(s) -- race conditions..? */

		retval = _do_setpass(pamh, user, pass_old, tpass, ctrl,
		                     remember);
		_pam_overwrite(pass_new);
		_pam_overwrite(pass_old);
		_pam_delete(tpass);
		pass_old = pass_new = NULL;
	} else {		/* something has broken with the module */
		_log_err(LOG_ALERT, pamh,
		         "password received unknown request");
		retval = PAM_ABORT;
	}

	D(("retval was %d", retval));

#ifdef USE_LCKPWDF
	ulckpwdf();
#endif
	return retval;
}


/* static module data */
#ifdef PAM_STATIC
struct pam_module _pam_unix_passwd_modstruct = {
    "pam_unix_passwd",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    pam_sm_chauthtok,
};
#endif

