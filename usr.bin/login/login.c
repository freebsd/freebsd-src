/*-
 * Copyright (c) 1980, 1987, 1988, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2002 Networks Associates Technologies, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed for the FreeBSD Project by
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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
 */

#if 0
#ifndef lint
static char sccsid[] = "@(#)login.c	8.4 (Berkeley) 4/2/94";
#endif
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * login [ name ]
 * login -h hostname	(for telnetd, etc.)
 * login -f name	(for pre-authenticated login: datakit, xterm, etc.)
 */

#include <sys/copyright.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <libutil.h>
#include <login_cap.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ttyent.h>
#include <unistd.h>

#include <security/pam_appl.h>
#include <security/openpam.h>

#include "login.h"
#include "pathnames.h"

static int		 auth_pam(void);
static void		 bail(int, int);
static int		 export(const char *);
static void		 export_pam_environment(void);
static int		 motd(const char *);
static void		 badlogin(char *);
static char		*getloginname(void);
static void		 pam_syslog(const char *);
static void		 pam_cleanup(void);
static void		 refused(const char *, const char *, int);
static const char	*stypeof(char *);
static void		 sigint(int);
static void		 timedout(int);
static void		 usage(void);

#define	TTYGRPNAME		"tty"			/* group to own ttys */
#define	DEFAULT_BACKOFF		3
#define	DEFAULT_RETRIES		10
#define	DEFAULT_PROMPT		"login: "
#define	DEFAULT_PASSWD_PROMPT	"Password:"
#define	TERM_UNKNOWN		"su"
#define	DEFAULT_WARN		(2L * 7L * 86400L)	/* Two weeks */
#define NO_SLEEP_EXIT		0
#define SLEEP_EXIT		5

/*
 * This bounds the time given to login.  Not a define so it can
 * be patched on machines where it's too small.
 */
static u_int		timeout = 300;

/* Buffer for signal handling of timeout */
static jmp_buf		 timeout_buf;

struct passwd		*pwd;
static int		 failures;

static char		*envinit[1];	/* empty environment list */

/*
 * Command line flags and arguments
 */
static int		 fflag;		/* -f: do not perform authentication */
static int		 hflag;		/* -h: login from remote host */
static char		*hostname;	/* hostname from command line */
static int		 pflag;		/* -p: preserve environment */

/*
 * User name
 */
static char		*username;	/* user name */
static char		*olduser;	/* previous user name */

/*
 * Prompts
 */
static char		 default_prompt[] = DEFAULT_PROMPT;
static const char	*prompt;
static char		 default_passwd_prompt[] = DEFAULT_PASSWD_PROMPT;
static const char	*passwd_prompt;

static char		*tty;

/*
 * PAM data
 */
static pam_handle_t	*pamh = NULL;
static struct pam_conv	 pamc = { openpam_ttyconv, NULL };
static int		 pam_err;
static int		 pam_silent = PAM_SILENT;
static int		 pam_cred_established;
static int		 pam_session_established;

int
main(int argc, char *argv[])
{
	struct group *gr;
	struct stat st;
	int retries, backoff;
	int ask, ch, cnt, quietlog, rootlogin, rval;
	uid_t uid, euid;
	gid_t egid;
	char *term;
	char *p, *ttyn;
	char tname[sizeof(_PATH_TTY) + 10];
	char *arg0;
	const char *shell = NULL;
	login_cap_t *lc = NULL;
	pid_t pid;

	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGHUP, SIG_IGN);
	if (setjmp(timeout_buf)) {
		if (failures)
			badlogin(username);
		(void)fprintf(stderr, "Login timed out after %d seconds\n",
		    timeout);
		bail(NO_SLEEP_EXIT, 0);
	}
	(void)signal(SIGALRM, timedout);
	(void)alarm(timeout);
	(void)setpriority(PRIO_PROCESS, 0, 0);

	openlog("login", LOG_ODELAY, LOG_AUTH);

	uid = getuid();
	euid = geteuid();
	egid = getegid();

	while ((ch = getopt(argc, argv, "fh:p")) != -1)
		switch (ch) {
		case 'f':
			fflag = 1;
			break;
		case 'h':
			if (uid != 0)
				errx(1, "-h option: %s", strerror(EPERM));
			if (strlen(optarg) >= MAXHOSTNAMELEN)
				errx(1, "-h option: %s: exceeds maximum "
				    "hostname size", optarg);
			hflag = 1;
			hostname = optarg;
			break;
		case 'p':
			pflag = 1;
			break;
		case '?':
		default:
			if (uid == 0)
				syslog(LOG_ERR, "invalid flag %c", ch);
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 0) {
		username = strdup(*argv);
		if (username == NULL)
			err(1, "strdup()");
		ask = 0;
	} else {
		ask = 1;
	}

	for (cnt = getdtablesize(); cnt > 2; cnt--)
		(void)close(cnt);

	/*
	 * Get current TTY
	 */
	ttyn = ttyname(STDIN_FILENO);
	if (ttyn == NULL || *ttyn == '\0') {
		(void)snprintf(tname, sizeof(tname), "%s??", _PATH_TTY);
		ttyn = tname;
	}
	if ((tty = strrchr(ttyn, '/')) != NULL)
		++tty;
	else
		tty = ttyn;

	/*
	 * Get "login-retries" & "login-backoff" from default class
	 */
	lc = login_getclass(NULL);
	prompt = login_getcapstr(lc, "prompt",
	    default_prompt, default_prompt);
	passwd_prompt = login_getcapstr(lc, "passwd_prompt",
	    default_passwd_prompt, default_passwd_prompt);
	retries = login_getcapnum(lc, "login-retries",
	    DEFAULT_RETRIES, DEFAULT_RETRIES);
	backoff = login_getcapnum(lc, "login-backoff",
	    DEFAULT_BACKOFF, DEFAULT_BACKOFF);
	login_close(lc);
	lc = NULL;

	/*
	 * Try to authenticate the user until we succeed or time out.
	 */
	for (cnt = 0;; ask = 1) {
		if (ask) {
			fflag = 0;
			if (olduser != NULL)
				free(olduser);
			olduser = username;
			username = getloginname();
		}
		rootlogin = 0;

		/*
		 * Note if trying multiple user names; log failures for
		 * previous user name, but don't bother logging one failure
		 * for nonexistent name (mistyped username).
		 */
		if (failures && strcmp(olduser, username) != 0) {
			if (failures > (pwd ? 0 : 1))
				badlogin(olduser);
		}

		/*
		 * Load the PAM policy and set some variables
		 */
		pam_err = pam_start("login", username, &pamc, &pamh);
		if (pam_err != PAM_SUCCESS) {
			pam_syslog("pam_start()");
			bail(NO_SLEEP_EXIT, 1);
		}
		pam_err = pam_set_item(pamh, PAM_TTY, tty);
		if (pam_err != PAM_SUCCESS) {
			pam_syslog("pam_set_item(PAM_TTY)");
			bail(NO_SLEEP_EXIT, 1);
		}
		pam_err = pam_set_item(pamh, PAM_RHOST, hostname);
		if (pam_err != PAM_SUCCESS) {
			pam_syslog("pam_set_item(PAM_RHOST)");
			bail(NO_SLEEP_EXIT, 1);
		}
		
		pwd = getpwnam(username);
		if (pwd != NULL && pwd->pw_uid == 0)
			rootlogin = 1;

		/*
		 * If the -f option was specified and the caller is
		 * root or the caller isn't changing their uid, don't
		 * authenticate.
		 */
		if (pwd != NULL && fflag &&
		    (uid == (uid_t)0 || uid == (uid_t)pwd->pw_uid)) {
			/* already authenticated */
			rval = 0;
		} else {
			fflag = 0;
			(void)setpriority(PRIO_PROCESS, 0, -4);
			rval = auth_pam();
			(void)setpriority(PRIO_PROCESS, 0, 0);
		}

		if (pwd && rval == 0)
			break;

		pam_cleanup();
		
		(void)printf("Login incorrect\n");
		failures++;

		/*
		 * Allow up to 'retry' (10) attempts, but start
		 * backing off after 'backoff' (3) attempts.
		 */
		if (++cnt > backoff) {
			if (cnt >= retries) {
				badlogin(username);
				bail(SLEEP_EXIT, 1);
			}
			sleep((u_int)((cnt - backoff) * 5));
		}
	}

	/* committed to login -- turn off timeout */
	(void)alarm((u_int)0);
	(void)signal(SIGHUP, SIG_DFL);

	endpwent();

	/*
	 * Establish the login class.
	 */
	lc = login_getpwclass(pwd);

	quietlog = login_getcapbool(lc, "hushlogin", 0);
	
	/*
	 * Switching needed for NFS with root access disabled.
	 *
	 * XXX: This change fails to modify the additional groups for the
	 * process, and as such, may restrict rights normally granted
	 * through those groups.
	 */
	(void)setegid(pwd->pw_gid);
	(void)seteuid(rootlogin ? 0 : pwd->pw_uid);
	if (!*pwd->pw_dir || chdir(pwd->pw_dir) < 0) {
		if (login_getcapbool(lc, "requirehome", 0))
			refused("Home directory not available", "HOMEDIR", 1);
		if (chdir("/") < 0) 
			refused("Cannot find root directory", "ROOTDIR", 1);
		if (!quietlog || *pwd->pw_dir)
			printf("No home directory.\nLogging in with home = \"/\".\n");
		pwd->pw_dir = strdup("/");
		if (pwd->pw_dir == NULL) {
			syslog(LOG_NOTICE, "strdup(): %m");
			bail(SLEEP_EXIT, 1);
		}
	}
	(void)seteuid(euid);
	(void)setegid(egid);
	if (!quietlog)
		quietlog = access(_PATH_HUSHLOGIN, F_OK) == 0;
	
        shell = login_getcapstr(lc, "shell", pwd->pw_shell, pwd->pw_shell);
	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = strdup(_PATH_BSHELL);
	if (pwd->pw_shell == NULL) {
		syslog(LOG_NOTICE, "strdup(): %m");
		bail(SLEEP_EXIT, 1);
	}
	if (*shell == '\0')   /* Not overridden */
		shell = pwd->pw_shell;
	if ((shell = strdup(shell)) == NULL) {
		syslog(LOG_NOTICE, "strdup(): %m");
		bail(SLEEP_EXIT, 1);
	}

	/*
	 * Set device protections, depending on what terminal the
	 * user is logged in. This feature is used on Suns to give
	 * console users better privacy.
	 */
	login_fbtab(tty, pwd->pw_uid, pwd->pw_gid);

	/*
	 * Clear flags of the tty.  None should be set, and when the
	 * user sets them otherwise, this can cause the chown to fail.
	 * Since it isn't clear that flags are useful on character
	 * devices, we just clear them.
	 */
	if (ttyn != tname && chflags(ttyn, 0) && errno != EOPNOTSUPP)
		syslog(LOG_ERR, "chflags(%s): %m", ttyn);
	if (ttyn != tname && chown(ttyn, pwd->pw_uid,
	    (gr = getgrnam(TTYGRPNAME)) ? gr->gr_gid : pwd->pw_gid))
		syslog(LOG_ERR, "chmod(%s): %m", ttyn);

	/*
	 * Exclude cons/vt/ptys only, assume dialup otherwise
	 * TODO: Make dialup tty determination a library call
	 * for consistency (finger etc.)
	 */
	if (hflag && isdialuptty(tty))
		syslog(LOG_INFO, "DIALUP %s, %s", tty, pwd->pw_name);

#ifdef LOGALL
	/*
	 * Syslog each successful login, so we don't have to watch
	 * hundreds of wtmp or lastlogin files.
	 */
	if (hflag)
		syslog(LOG_INFO, "login from %s on %s as %s",
		       hostname, tty, pwd->pw_name);
	else
		syslog(LOG_INFO, "login on %s as %s",
		       tty, pwd->pw_name);
#endif

	/*
	 * If fflag is on, assume caller/authenticator has logged root
	 * login.
	 */
	if (rootlogin && fflag == 0) {
		if (hflag)
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s FROM %s",
			    username, tty, hostname);
		else
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s",
			    username, tty);
	}

	/*
	 * Destroy environment unless user has requested its
	 * preservation - but preserve TERM in all cases
	 */
	term = getenv("TERM");
	if (!pflag)
		environ = envinit;
	if (term != NULL)
		setenv("TERM", term, 0);

	/*
	 * PAM modules might add supplementary groups during pam_setcred().
	 */
	if (setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETGROUP) != 0) {
                syslog(LOG_ERR, "setusercontext() failed - exiting");
		bail(NO_SLEEP_EXIT, 1);
	}

	pam_err = pam_setcred(pamh, pam_silent|PAM_ESTABLISH_CRED);
	if (pam_err != PAM_SUCCESS) {
		pam_syslog("pam_setcred()");
		bail(NO_SLEEP_EXIT, 1);
	}
	pam_cred_established = 1;
	
	pam_err = pam_open_session(pamh, pam_silent);
	if (pam_err != PAM_SUCCESS) {
		pam_syslog("pam_open_session()");
		bail(NO_SLEEP_EXIT, 1);
	}
	pam_session_established = 1;

	/*
	 * We must fork() before setuid() because we need to call
	 * pam_close_session() as root.
	 */
	pid = fork();
	if (pid < 0) {
		err(1, "fork");
	} else if (pid != 0) {
		/*
		 * Parent: wait for child to finish, then clean up
		 * session.
		 */
		wait(NULL);
		bail(NO_SLEEP_EXIT, 0);
	}

	/*
	 * NOTICE: We are now in the child process!
	 */
	
	/*
	 * Add any environment variables the PAM modules may have set.
	 */
	export_pam_environment();

	/*
	 * We're done with PAM now; our parent will deal with the rest.
	 */
	pam_end(pamh, 0);
	pamh = NULL;

	/*
	 * We don't need to be root anymore, so set the login name and
	 * the UID.
	 */
	if (setlogin(username) != 0) {
                syslog(LOG_ERR, "setlogin(%s): %m - exiting", username);
		bail(NO_SLEEP_EXIT, 1);
	}
	if (setusercontext(lc, pwd, pwd->pw_uid,
	    LOGIN_SETALL & ~(LOGIN_SETLOGIN|LOGIN_SETGROUP)) != 0) {
                syslog(LOG_ERR, "setusercontext() failed - exiting");
		exit(1);
	}

	(void)setenv("SHELL", pwd->pw_shell, 1);
	(void)setenv("HOME", pwd->pw_dir, 1);
	(void)setenv("TERM", stypeof(tty), 0);
	(void)setenv("LOGNAME", username, 1);
	(void)setenv("USER", username, 1);
	(void)setenv("PATH", rootlogin ? _PATH_STDPATH : _PATH_DEFPATH, 0);

	if (!quietlog) {
		const char *cw;

		cw = login_getcapstr(lc, "copyright", NULL, NULL);
		if (cw == NULL || motd(cw) == -1)
			(void)printf("%s", copyright);

		(void)printf("\n");

		cw = login_getcapstr(lc, "welcome", NULL, NULL);
		if (cw != NULL && access(cw, F_OK) == 0)
			motd(cw);
		else
			motd(_PATH_MOTDFILE);

		if (login_getcapbool(lc, "nocheckmail", 0) == 0) {
			/* $MAIL may have been set by class. */
			cw = getenv("MAIL");
			if (cw == NULL) {
				asprintf((char **)&cw, "%s/%s",
				    _PATH_MAILDIR, pwd->pw_name);
			}
			if (cw && stat(cw, &st) == 0 && st.st_size != 0)
				(void)printf("You have %smail.\n",
				    (st.st_mtime > st.st_atime) ? "new " : "");
			if (getenv("MAIL") == NULL)
				free((char *)cw);
		}
	}

	login_close(lc);

	(void)signal(SIGALRM, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGTSTP, SIG_IGN);

	/*
	 * Login shells have a leading '-' in front of argv[0]
	 */
	p = strrchr(pwd->pw_shell, '/');
	if (asprintf(&arg0, "-%s", p ? p + 1 : pwd->pw_shell) >= MAXPATHLEN) {
		syslog(LOG_ERR, "user: %s: shell exceeds maximum pathname size",
		    username);
		errx(1, "shell exceeds maximum pathname size");
	} else if (arg0 == NULL) {
		err(1, "asprintf()");
	}

	execlp(shell, arg0, (char *)0);
	err(1, "%s", shell);
	
	/*
	 * That's it, folks!
	 */
}

/*
 * Attempt to authenticate the user using PAM.  Returns 0 if the user is
 * authenticated, or 1 if not authenticated.  If some sort of PAM system
 * error occurs (e.g., the "/etc/pam.conf" file is missing) then this
 * function returns -1.  This can be used as an indication that we should
 * fall back to a different authentication mechanism.
 */
static int
auth_pam(void)
{
	const char *tmpl_user;
	const void *item;
	int rval;

	pam_err = pam_authenticate(pamh, pam_silent);
	switch (pam_err) {

	case PAM_SUCCESS:
		/*
		 * With PAM we support the concept of a "template"
		 * user.  The user enters a login name which is
		 * authenticated by PAM, usually via a remote service
		 * such as RADIUS or TACACS+.  If authentication
		 * succeeds, a different but related "template" name
		 * is used for setting the credentials, shell, and
		 * home directory.  The name the user enters need only
		 * exist on the remote authentication server, but the
		 * template name must be present in the local password
		 * database.
		 *
		 * This is supported by two various mechanisms in the
		 * individual modules.  However, from the application's
		 * point of view, the template user is always passed
		 * back as a changed value of the PAM_USER item.
		 */
		pam_err = pam_get_item(pamh, PAM_USER, &item);
		if (pam_err == PAM_SUCCESS) {
			tmpl_user = (const char *)item;
			if (strcmp(username, tmpl_user) != 0)
				pwd = getpwnam(tmpl_user);
		} else {
			pam_syslog("pam_get_item(PAM_USER)");
		}
		rval = 0;
		break;

	case PAM_AUTH_ERR:
	case PAM_USER_UNKNOWN:
	case PAM_MAXTRIES:
		rval = 1;
		break;

	default:
		pam_syslog("pam_authenticate()");
		rval = -1;
		break;
	}

	if (rval == 0) {
		pam_err = pam_acct_mgmt(pamh, pam_silent);
		switch (pam_err) {
		case PAM_SUCCESS:
			break;
		case PAM_NEW_AUTHTOK_REQD:
			pam_err = pam_chauthtok(pamh,
			    pam_silent|PAM_CHANGE_EXPIRED_AUTHTOK);
			if (pam_err != PAM_SUCCESS) {
				pam_syslog("pam_chauthtok()");
				rval = 1;
			}
			break;
		default:
			pam_syslog("pam_acct_mgmt()");
			rval = 1;
			break;
		}
	}

	if (rval != 0) {
		pam_end(pamh, pam_err);
		pamh = NULL;
	}
	return (rval);
}

/*
 * Export any environment variables PAM modules may have set
 */
static void
export_pam_environment()
{
	char **pam_env;
	char **pp;

	pam_env = pam_getenvlist(pamh);
	if (pam_env != NULL) {
		for (pp = pam_env; *pp != NULL; pp++) {
			(void)export(*pp);
			free(*pp);
		}
	}
}

/*
 * Perform sanity checks on an environment variable:
 * - Make sure there is an '=' in the string.
 * - Make sure the string doesn't run on too long.
 * - Do not export certain variables.  This list was taken from the
 *   Solaris pam_putenv(3) man page.
 * Then export it.
 */
static int
export(const char *s)
{
	static const char *noexport[] = {
		"SHELL", "HOME", "LOGNAME", "MAIL", "CDPATH",
		"IFS", "PATH", NULL
	};
	const char **pp;
	size_t n;

	if (strlen(s) > 1024 || strchr(s, '=') == NULL)
		return (0);
	if (strncmp(s, "LD_", 3) == 0)
		return (0);
	for (pp = noexport; *pp != NULL; pp++) {
		n = strlen(*pp);
		if (s[n] == '=' && strncmp(s, *pp, n) == 0)
			return (0);
	}
	(void)putenv(s);
	return (1);
}

static void
usage()
{

	(void)fprintf(stderr, "usage: login [-fp] [-h hostname] [username]\n");
	exit(1);
}

/*
 * Prompt user and read login name from stdin.
 */
static char *
getloginname()
{
	char *nbuf, *p;
	int ch;

	nbuf = malloc(MAXLOGNAME);
	if (nbuf == NULL)
		err(1, "malloc()");
	do {
		(void)printf("%s", prompt);
		for (p = nbuf; (ch = getchar()) != '\n'; ) {
			if (ch == EOF) {
				badlogin(username);
				bail(NO_SLEEP_EXIT, 0);
			}
			if (p < nbuf + MAXLOGNAME - 1)
				*p++ = ch;
		}
	} while (p == nbuf);
	
	*p = '\0';
	if (nbuf[0] == '-') {
		pam_silent = 0;
		memmove(nbuf, nbuf + 1, strlen(nbuf));
	} else {
		pam_silent = PAM_SILENT;
	}
	return nbuf;
}

/*
 * SIGINT handler for motd().
 */
static volatile int motdinterrupt;
static void
sigint(int signo __unused)
{
	motdinterrupt = 1;
}

/*
 * Display the contents of a file (such as /etc/motd).
 */
static int
motd(const char *motdfile)
{
	sig_t oldint;
	FILE *f;
	int ch;

	if ((f = fopen(motdfile, "r")) == NULL)
		return (-1);
	motdinterrupt = 0;
	oldint = signal(SIGINT, sigint);
	while ((ch = fgetc(f)) != EOF && !motdinterrupt)
		putchar(ch);
	signal(SIGINT, oldint);
	if (ch != EOF || ferror(f)) {
		fclose(f);
		return (-1);
	}
	fclose(f);
	return (0);
}

/*
 * SIGALRM handler, to enforce login prompt timeout.
 *
 * XXX This can potentially confuse the hell out of PAM.  We should
 * XXX instead implement a conversation function that returns
 * XXX PAM_CONV_ERR when interrupted by a signal, and have the signal
 * XXX handler just set a flag.
 */
static void
timedout(int signo __unused)
{

	longjmp(timeout_buf, signo);
}

void
badlogin(char *name)
{

	if (failures == 0)
		return;
	if (hflag) {
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s FROM %s",
		    failures, failures > 1 ? "S" : "", hostname);
		syslog(LOG_AUTHPRIV|LOG_NOTICE,
		    "%d LOGIN FAILURE%s FROM %s, %s",
		    failures, failures > 1 ? "S" : "", hostname, name);
	} else {
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s ON %s",
		    failures, failures > 1 ? "S" : "", tty);
		syslog(LOG_AUTHPRIV|LOG_NOTICE,
		    "%d LOGIN FAILURE%s ON %s, %s",
		    failures, failures > 1 ? "S" : "", tty, name);
	}
	failures = 0;
}

const char *
stypeof(char *ttyid)
{
	struct ttyent *t;

	if (ttyid != NULL && *ttyid != '\0') {
		t = getttynam(ttyid);
		if (t != NULL && t->ty_type != NULL)
			return (t->ty_type);
	}
	return (TERM_UNKNOWN);
}

void
refused(const char *msg, const char *rtype, int lout)
{

	if (msg != NULL)
	    printf("%s.\n", msg);
	if (hflag)
		syslog(LOG_NOTICE, "LOGIN %s REFUSED (%s) FROM %s ON TTY %s",
		    pwd->pw_name, rtype, hostname, tty);
	else
		syslog(LOG_NOTICE, "LOGIN %s REFUSED (%s) ON TTY %s",
		    pwd->pw_name, rtype, tty);
	if (lout)
		bail(SLEEP_EXIT, 1);
}

/*
 * Log a PAM error
 */
void
pam_syslog(const char *msg)
{
	syslog(LOG_ERR, "%s: %s", msg, pam_strerror(pamh, pam_err));
}

/*
 * Shut down PAM
 */
void
pam_cleanup()
{

	if (pamh != NULL) {
		if (pam_session_established) {
			pam_err = pam_close_session(pamh, 0);
			if (pam_err != PAM_SUCCESS)
				pam_syslog("pam_close_session()");
		}
		pam_session_established = 0;
		if (pam_cred_established) {
			pam_err = pam_setcred(pamh, pam_silent|PAM_DELETE_CRED);
			if (pam_err != PAM_SUCCESS)
				pam_syslog("pam_setcred()");
		}
		pam_cred_established = 0;
		pam_end(pamh, pam_err);
		pamh = NULL;
	}
}

/*
 * Exit, optionally after sleeping a few seconds
 */
void
bail(int sec, int eval)
{

	pam_cleanup();
	(void)sleep(sec);
	exit(eval);
}
