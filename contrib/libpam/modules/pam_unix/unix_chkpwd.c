/*
 * $Id: unix_chkpwd.c,v 1.3 2001/02/11 06:33:53 agmorgan Exp $
 * $FreeBSD$
 *
 * This program is designed to run setuid(root) or with sufficient
 * privilege to read all of the unix password databases. It is designed
 * to provide a mechanism for the current user (defined by this
 * process' uid) to verify their own password.
 *
 * The password is read from the standard input. The exit status of
 * this program indicates whether the user is authenticated or not.
 *
 * Copyright information is located at the end of the file.
 *
 */

#include <security/_pam_aconf.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>
#include <signal.h>

#define MAXPASS		200	/* the maximum length of a password */

#include <security/_pam_macros.h>

#include "md5.h"

extern char *crypt(const char *key, const char *salt);
extern char *bigcrypt(const char *key, const char *salt);

#define UNIX_PASSED	0
#define UNIX_FAILED	1

/* syslogging function for errors and other information */

static void _log_err(int err, const char *format,...)
{
	va_list args;

	va_start(args, format);
	openlog("unix_chkpwd", LOG_CONS | LOG_PID, LOG_AUTH);
	vsyslog(err, format, args);
	va_end(args);
	closelog();
}

static void su_sighandler(int sig)
{
	if (sig > 0) {
		_log_err(LOG_NOTICE, "caught signal %d.", sig);
		exit(sig);
	}
}

static void setup_signals(void)
{
	struct sigaction action;	/* posix signal structure */

	/*
	 * Setup signal handlers
	 */
	(void) memset((void *) &action, 0, sizeof(action));
	action.sa_handler = su_sighandler;
	action.sa_flags = SA_RESETHAND;
	(void) sigaction(SIGILL, &action, NULL);
	(void) sigaction(SIGTRAP, &action, NULL);
	(void) sigaction(SIGBUS, &action, NULL);
	(void) sigaction(SIGSEGV, &action, NULL);
	action.sa_handler = SIG_IGN;
	action.sa_flags = 0;
	(void) sigaction(SIGTERM, &action, NULL);
	(void) sigaction(SIGHUP, &action, NULL);
	(void) sigaction(SIGINT, &action, NULL);
	(void) sigaction(SIGQUIT, &action, NULL);
}

static int _unix_verify_password(const char *name, const char *p, int opt)
{
	struct passwd *pwd = NULL;
	struct spwd *spwdent = NULL;
	char *salt = NULL;
	char *pp = NULL;
	int retval = UNIX_FAILED;

	/* UNIX passwords area */
	setpwent();
	pwd = getpwnam(name);	/* Get password file entry... */
	endpwent();
	if (pwd != NULL) {
		if (strcmp(pwd->pw_passwd, "x") == 0) {
			/*
			 * ...and shadow password file entry for this user,
			 * if shadowing is enabled
			 */
			setspent();
			spwdent = getspnam(name);
			endspent();
			if (spwdent != NULL)
				salt = x_strdup(spwdent->sp_pwdp);
			else
				pwd = NULL;
		} else {
			if (strcmp(pwd->pw_passwd, "*NP*") == 0) {	/* NIS+ */
				uid_t save_uid;

				save_uid = geteuid();
				seteuid(pwd->pw_uid);
				spwdent = getspnam(name);
				seteuid(save_uid);

				salt = x_strdup(spwdent->sp_pwdp);
			} else {
				salt = x_strdup(pwd->pw_passwd);
			}
		}
	}
	if (pwd == NULL || salt == NULL) {
		_log_err(LOG_ALERT, "check pass; user unknown");
		p = NULL;
		return retval;
	}

	if (strlen(salt) == 0)
		return (opt == 0) ? UNIX_FAILED : UNIX_PASSED;

	/* the moment of truth -- do we agree with the password? */
	retval = UNIX_FAILED;
	if (!strncmp(salt, "$1$", 3)) {
		pp = Goodcrypt_md5(p, salt);
		if (strcmp(pp, salt) == 0) {
			retval = UNIX_PASSED;
		} else {
			pp = Brokencrypt_md5(p, salt);
			if (strcmp(pp, salt) == 0)
				retval = UNIX_PASSED;
		}
	} else {
		pp = bigcrypt(p, salt);
		if (strcmp(pp, salt) == 0) {
			retval = UNIX_PASSED;
		}
	}
	p = NULL;		/* no longer needed here */

	/* clean up */
	{
		char *tp = pp;
		if (pp != NULL) {
			while (tp && *tp)
				*tp++ = '\0';
		}
		pp = tp = NULL;
	}

	return retval;
}

static char *getuidname(uid_t uid)
{
	struct passwd *pw;
	static char username[32];

	pw = getpwuid(uid);
	if (pw == NULL)
		return NULL;

	memset(username, 0, 32);
	strncpy(username, pw->pw_name, 32);
	username[31] = '\0';
	
	return username;
}

int main(int argc, char *argv[])
{
	char pass[MAXPASS + 1];
	char option[8];
	int npass, opt;
	int force_failure = 0;
	int retval = UNIX_FAILED;
	char *user;

	/*
	 * Catch or ignore as many signal as possible.
	 */
	setup_signals();

	/*
	 * we establish that this program is running with non-tty stdin.
	 * this is to discourage casual use. It does *NOT* prevent an
	 * intruder from repeatadly running this program to determine the
	 * password of the current user (brute force attack, but one for
	 * which the attacker must already have gained access to the user's
	 * account).
	 */

	if (isatty(STDIN_FILENO)) {

		_log_err(LOG_NOTICE
		      ,"inappropriate use of Unix helper binary [UID=%d]"
			 ,getuid());
		fprintf(stderr
		 ,"This binary is not designed for running in this way\n"
		      "-- the system administrator has been informed\n");
		sleep(10);	/* this should discourage/annoy the user */
		return UNIX_FAILED;
	}

	/*
	 * determine the current user's name is
	 */
	user = getuidname(getuid());
	if (argc == 2) {
	    /* if the caller specifies the username, verify that user
	       matches it */
	    if (strcmp(user, argv[1])) {
		force_failure = 1;
	    }
	}

	/* read the nollok/nonull option */

	npass = read(STDIN_FILENO, option, 8);

	if (npass < 0) {
		_log_err(LOG_DEBUG, "no option supplied");
		return UNIX_FAILED;
	} else {
		option[7] = '\0';
		if (strncmp(option, "nullok", 8) == 0)
			opt = 1;
		else
			opt = 0;
	}

	/* read the password from stdin (a pipe from the pam_unix module) */

	npass = read(STDIN_FILENO, pass, MAXPASS);

	if (npass < 0) {	/* is it a valid password? */

		_log_err(LOG_DEBUG, "no password supplied");

	} else if (npass >= MAXPASS) {

		_log_err(LOG_DEBUG, "password too long");

	} else {
		if (npass == 0) {
			/* the password is NULL */

			retval = _unix_verify_password(user, NULL, opt);

		} else {
			/* does pass agree with the official one? */

			pass[npass] = '\0';	/* NUL terminate */
			retval = _unix_verify_password(user, pass, opt);

		}
	}

	memset(pass, '\0', MAXPASS);	/* clear memory of the password */

	/* return pass or fail */

	if ((retval != UNIX_PASSED) || force_failure) {
	    return UNIX_FAILED;
	} else {
	    return UNIX_PASSED;
	}
}

/*
 * Copyright (c) Andrew G. Morgan, 1996. All rights reserved
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
