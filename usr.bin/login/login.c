/*-
 * Copyright (c) 1980, 1987, 1988, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
static char copyright[] =
"@(#) Copyright (c) 1980, 1987, 1988, 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static char sccsid[] = "@(#)login.c	8.4 (Berkeley) 4/2/94";
#endif /* not lint */

/*
 * login [ name ]
 * login -h hostname	(for telnetd, etc.)
 * login -f name	(for pre-authenticated login: datakit, xterm, etc.)
 */

#include <sys/copyright.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ttyent.h>
#include <unistd.h>
#include <utmp.h>

#ifdef LOGIN_CAP
#include <login_cap.h>
#else /* Undef AUTH as well */
#undef LOGIN_CAP_AUTH
#endif

/*
 * If LOGIN_CAP_AUTH is activated:
 * kerberose & skey logins are runtime selected via login
 * login_getstyle() and authentication types for login classes
 * The actual login itself is handled via /usr/libexec/login_<style>
 * Valid styles are determined by the auth-type=style,style entries
 * in the login class.
 */
#ifdef LOGIN_CAP_AUTH
#undef KERBEROS
#undef SKEY
#endif /* LOGIN_CAP_AUTH */

#ifdef	SKEY
#include <skey.h>
#endif /* SKEY */

#include "pathnames.h"

void	 badlogin __P((char *));
void	 checknologin __P((void));
void	 dolastlog __P((int));
void	 getloginname __P((void));
void	 motd __P((char *));
int	 rootterm __P((char *));
void	 sigint __P((int));
void	 sleepexit __P((int));
void	 refused __P((char *,char *,int));
char	*stypeof __P((char *));
void	 timedout __P((int));
void     login_fbtab __P((char *, uid_t, gid_t));
#ifdef KERBEROS
int	 klogin __P((struct passwd *, char *, char *, char *));
#endif

extern void login __P((struct utmp *));

#define	TTYGRPNAME	"tty"		/* name of group to own ttys */
#define	DEFAULT_BACKOFF	3
#define	DEFAULT_RETRIES	10

/*
 * This bounds the time given to login.  Not a define so it can
 * be patched on machines where it's too small.
 */
u_int	timeout = 300;

#ifdef KERBEROS
int	notickets = 1;
int	noticketsdontcomplain = 1;
char	*instance;
char	*krbtkfile_env;
int	authok;
#endif

struct	passwd *pwd;
int	failures;
char	*term, *envinit[1], *hostname, *username, *tty;
char    full_hostname[MAXHOSTNAMELEN];

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char **environ;
	struct group *gr;
	struct stat st;
	struct timeval tp;
	struct utmp utmp;
	int rootok, retries, backoff;
	int ask, ch, cnt, fflag, hflag, pflag, quietlog, rootlogin, rval;
	int changepass;
	time_t warntime;
	uid_t uid;
	char *domain, *p, *ep, *salt, *ttyn;
	char tbuf[MAXPATHLEN + 2], tname[sizeof(_PATH_TTY) + 10];
	char localhost[MAXHOSTNAMELEN];
	char *shell = NULL;
#ifdef LOGIN_CAP
	login_cap_t *lc = NULL;
#ifdef LOGIN_CAP_AUTH
	char *style, *authtype;
	char *auth_method = NULL;
	char *instance = NULL;
	int authok;
#endif /* LOGIN_CAP_AUTH */
#endif /* LOGIN_CAP */
#ifdef SKEY
	int permit_passwd = 0;
#endif /* SKEY */

	(void)signal(SIGALRM, timedout);
	(void)alarm(timeout);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)setpriority(PRIO_PROCESS, 0, 0);

	openlog("login", LOG_ODELAY, LOG_AUTH);

	/*
	 * -p is used by getty to tell login not to destroy the environment
	 * -f is used to skip a second login authentication
	 * -h is used by other servers to pass the name of the remote
	 *    host to login so that it may be placed in utmp and wtmp
	 */
	*full_hostname = '\0';
	domain = NULL;
	term = NULL;
	if (gethostname(localhost, sizeof(localhost)) < 0)
		syslog(LOG_ERR, "couldn't get local hostname: %m");
	else
		domain = strchr(localhost, '.');

	fflag = hflag = pflag = 0;
	uid = getuid();
	while ((ch = getopt(argc, argv, "fh:p")) != -1)
		switch (ch) {
		case 'f':
			fflag = 1;
			break;
		case 'h':
			if (uid)
				errx(1, "-h option: %s", strerror(EPERM));
			hflag = 1;
			strncpy(full_hostname, optarg, sizeof(full_hostname)-1);
			if (domain && (p = strchr(optarg, '.')) &&
			    strcasecmp(p, domain) == 0)
				*p = 0;
			if (strlen(optarg) > UT_HOSTSIZE) {
				struct hostent *hp = gethostbyname(optarg);

				if (hp != NULL) {
					struct in_addr in;

					memmove(&in, hp->h_addr, sizeof(in));
					optarg = strdup(inet_ntoa(in));
				} else
					optarg = "invalid hostname";
			}
			hostname = optarg;
			break;
		case 'p':
			pflag = 1;
			break;
		case '?':
		default:
			if (!uid)
				syslog(LOG_ERR, "invalid flag %c", ch);
			(void)fprintf(stderr,
			    "usage: login [-fp] [-h hostname] [username]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (*argv) {
		username = *argv;
		ask = 0;
	} else
		ask = 1;

	for (cnt = getdtablesize(); cnt > 2; cnt--)
		(void)close(cnt);

	ttyn = ttyname(STDIN_FILENO);
	if (ttyn == NULL || *ttyn == '\0') {
		(void)snprintf(tname, sizeof(tname), "%s??", _PATH_TTY);
		ttyn = tname;
	}
	if ((tty = strrchr(ttyn, '/')) != NULL)
		++tty;
	else
		tty = ttyn;

#ifdef LOGIN_CAP_AUTH
	authtype = hostname ? "rlogin" : "login";
#endif
#ifdef LOGIN_CAP
	/*
	 * Get "login-retries" & "login-backoff" from default class
	 */
	lc = login_getclass(NULL);
	retries = login_getcapnum(lc, "login-retries", DEFAULT_RETRIES, DEFAULT_RETRIES);
	backoff = login_getcapnum(lc, "login-backoff", DEFAULT_BACKOFF, DEFAULT_BACKOFF);
	login_close(lc);
	lc = NULL;
#else
	retries = DEFAULT_RETRIES;
	backoff = DEFAULT_BACKOFF;
#endif

	for (cnt = 0;; ask = 1) {
		if (ask) {
			fflag = 0;
			getloginname();
		}
		rootlogin = 0;
		rootok = rootterm(tty); /* Default (auth may change) */
#ifdef LOGIN_CAP_AUTH
		authok = 0;
		if (auth_method = strchr(username, ':')) {
			*auth_method = '\0';
			auth_method++;
			if (*auth_method == '\0')
				auth_method = NULL;
		}
		/*
		 * We need to do this regardless of whether
		 * kerberos is available.
		 */
		if ((instance = strchr(username, '.')) != NULL) {
			if (strncmp(instance, ".root", 5) == 0)
				rootlogin = 1;
			*instance++ = '\0';
		} else
			instance = "";
#else /* !LOGIN_CAP_AUTH */
#ifdef KERBEROS
		if ((instance = strchr(username, '.')) != NULL) {
			if (strncmp(instance, ".root", 5) == 0)
				rootlogin = 1;
			*instance++ = '\0';
		} else
			instance = "";
#endif /* KERBEROS */
#endif /* LOGIN_CAP_AUTH */

		if (strlen(username) > UT_NAMESIZE)
			username[UT_NAMESIZE] = '\0';

		/*
		 * Note if trying multiple user names; log failures for
		 * previous user name, but don't bother logging one failure
		 * for nonexistent name (mistyped username).
		 */
		if (failures && strcmp(tbuf, username)) {
			if (failures > (pwd ? 0 : 1))
				badlogin(tbuf);
			failures = 0;
		}
		(void)strcpy(tbuf, username);

		if ((pwd = getpwnam(username)) != NULL)
			salt = pwd->pw_passwd;
		else
			salt = "xx";

#ifdef LOGIN_CAP
		/*
		 * Establish the class now, before we might goto
		 * within the next block. pwd can be NULL since it
		 * falls back to the "default" class if it is.
		 */
		lc = login_getpwclass(pwd);
#endif /* LOGIN_CAP */

		/*
		 * if we have a valid account name, and it doesn't have a
		 * password, or the -f option was specified and the caller
		 * is root or the caller isn't changing their uid, don't
		 * authenticate.
		 */
		rval = 1;
		if (pwd != NULL) {
			if (pwd->pw_uid == 0)
				rootlogin = 1;

			if (fflag && (uid == (uid_t)0 ||
				      uid == (uid_t)pwd->pw_uid)) {
				/* already authenticated */
				break;
			} else if (pwd->pw_passwd[0] == '\0') {
				if (!rootlogin || rootok) {
					/* pretend password okay */
					rval = 0;
					goto ttycheck;
				}
			}
		}

		fflag = 0;

		(void)setpriority(PRIO_PROCESS, 0, -4);

#ifdef LOGIN_CAP_AUTH
		/*
		 * This hands off authorization to an authorization program,
		 * depending on the styles available for the "auth-login",
		 * auth-rlogin (or default) authorization styles.
		 * We do this regardless of whether an account exists so that
		 * the remote user cannot tell a "real" from an invented
		 * account name. If we don't have an account we just fall
		 * back to the first method for the "default" class.
		 */
		if (!(style = login_getstyle(lc, auth_method, authtype))) {

			/*
			 * No available authorization method
			 */
			rval = 1;
			(void)printf("No auth method available for %s.\n",
				     authtype);
		} else {

			/*
			 * Put back the kerberos instance, if any was given.
			 * Don't worry about the non-kerberos case here, since
			 * if kerberos is not available or not selected and an
			 * instance is given at the login prompt, su or rlogin -l,
			 * then anything else should fail as well.
			 */
			if (*instance)
				*(instance - 1) = '.';

			rval = authenticate(username,
					    lc ? lc->lc_class : "default",
					    style, authtype);
			/* Junk it again */
			if (*instance)
				*(instance - 1) = '\0';
		}

		if (!rval) {
			char * approvp;
		    
			/*
			 * If authentication succeeds, run any approval
			 * program, if applicable for this class.
			 */
			approvep = login_getcapstr(lc, "approve", NULL, NULL);
			rval = 1; /* Assume bad login again */

			if (approvep==NULL ||
			    auth_script(approvep, approvep, username,
					lc->lc_class, 0) == 0) {
				int     r;

				r = auth_scan(AUTH_OKAY);
				/*
				 * See what the authorize program says
				 */
				if (r != AUTH_NONE) {
					rval = 0;

					if (!rootok && (r & AUTH_ROOTOKAY))
						rootok = 1; /* root approved */
					else
						rootlogin = 0;

					if (!authok && (r & AUTH_SECURE))
						authok = 1; /* secure */
				}
			}
		}
#else /* !LOGIN_CAP_AUTH */
#ifdef SKEY
		permit_passwd = skeyaccess(username, tty,
					   hostname ? full_hostname : NULL,
					   NULL);
		p = skey_getpass("Password:", pwd, permit_passwd);
		ep = skey_crypt(p, salt, pwd, permit_passwd);
#else /* !SKEY */
		p = getpass("Password:");
		ep = crypt(p, salt);
#endif/* SKEY */

		if (pwd) {
#ifdef KERBEROS
#ifdef SKEY
			/*
			 * Do not allow user to type in kerberos password
			 * over the net (actually, this is ok for encrypted
			 * links, but we have no way of determining if the
			 * link is encrypted.
			 */
			if (!permit_passwd) {
				rval = 1;		/* failed */
			} else
#endif /* SKEY */
			rval = klogin(pwd, instance, localhost, p);
			if (rval != 0 && rootlogin && pwd->pw_uid != 0)
				rootlogin = 0;
			if (rval == 0)
				authok = 1; /* kerberos authenticated ok */
			else if (rval == 1) /* fallback to unix passwd */
				rval = strcmp(ep, pwd->pw_passwd);
#else /* !KERBEROS */
			rval = strcmp(ep, pwd->pw_passwd);
#endif /* KERBEROS */
		}

		/* clear entered password */
		memset(p, 0, strlen(p));
#endif /* LOGIN_CAP_AUTH */

		(void)setpriority(PRIO_PROCESS, 0, 0);

#ifdef LOGIN_CAP_AUTH
		if (rval)
			auth_rmfiles();
#endif
	ttycheck:
		/*
		 * If trying to log in as root without Kerberos,
		 * but with insecure terminal, refuse the login attempt.
		 */
		if (pwd && !rval) {
#if defined(KERBEROS) || defined(LOGIN_CAP_AUTH)
			if (authok == 0 && rootlogin && !rootok)
#else
			if (rootlogin && !rootok)
#endif
				refused(NULL, "NOROOT", 0);
			else	/* valid password & authenticated */
				break;
		}

		(void)printf("Login incorrect\n");
		failures++;

		/*
		 * we allow up to 'retry' (10) tries,
		 * but after 'backoff' (3) we start backing off
		 */
		if (++cnt > backoff) {
			if (cnt >= retries) {
				badlogin(username);
				sleepexit(1);
			}
			sleep((u_int)((cnt - 3) * 5));
		}
	}

	/* committed to login -- turn off timeout */
	(void)alarm((u_int)0);

	endpwent();

	/* if user not super-user, check for disabled logins */
#ifdef LOGIN_CAP
	if (!rootlogin)
		auth_checknologin(lc);
#else
	if (!rootlogin)
		checknologin();
#endif

#ifdef LOGIN_CAP
	quietlog = login_getcapbool(lc, "hushlogin", 0);
#else
	quietlog = 0;
#endif
	if (!*pwd->pw_dir || chdir(pwd->pw_dir) < 0) {
#ifdef LOGIN_CAP
		if (login_getcapbool(lc, "requirehome", !rootlogin))
			refused("Home directory not available", "HOMEDIR", 1);
#endif
		if (chdir("/") < 0) 
			refused("Cannot find root directory", "ROOTDIR", 1);
		pwd->pw_dir = "/";
		if (!quietlog || *pwd->pw_dir)
			printf("No home directory.\nLogging in with home = \"/\".\n");
	}
	if (!quietlog)
		quietlog = access(_PATH_HUSHLOGIN, F_OK) == 0;

	if (pwd->pw_change || pwd->pw_expire)
		(void)gettimeofday(&tp, (struct timezone *)NULL);

#define DEFAULT_WARN  (2L * 7L & 86400L)  /* Two weeks */

#ifdef LOGIN_CAP
	warntime = login_getcaptime(lc, "warnpassword",
				    DEFAULT_WARN, DEFAULT_WARN);
#else
	warntime = DEFAULT_WARN;
#endif

	changepass=0;
	if (pwd->pw_change) {
		if (tp.tv_sec >= pwd->pw_change) {
			(void)printf("Sorry -- your password has expired.\n");
			changepass=1;
			syslog(LOG_INFO,
			       "%s Password expired - forcing change",
			       pwd->pw_name);
		} else if (pwd->pw_change - tp.tv_sec < warntime && !quietlog)
		    (void)printf("Warning: your password expires on %s",
				 ctime(&pwd->pw_change));
	}

#ifdef LOGIN_CAP
	warntime = login_getcaptime(lc, "warnexpire",
				    DEFAULT_WARN, DEFAULT_WARN);
#else
	warntime = DEFAULT_WARN;
#endif

	if (pwd->pw_expire) {
		if (tp.tv_sec >= pwd->pw_expire) {
			refused("Sorry -- your account has expired",
				"EXPIRED", 1);
		} else if (pwd->pw_expire - tp.tv_sec < warntime && !quietlog)
		    (void)printf("Warning: your account expires on %s",
				 ctime(&pwd->pw_expire));
	}

#ifdef LOGIN_CAP
	if (lc != NULL) {
		if (hostname) {
			struct hostent *hp = gethostbyname(full_hostname);

			if (hp == NULL)
				optarg = NULL;
			else {
				struct in_addr in;
				memmove(&in, hp->h_addr, sizeof(in));
				optarg = strdup(inet_ntoa(in));
			}
			if (!auth_hostok(lc, full_hostname, optarg))
				refused("Permission denied", "HOST", 1);
		}

		if (!auth_ttyok(lc, tty))
			refused("Permission denied", "TTY", 1);

		if (!auth_timeok(lc, time(NULL)))
			refused("Logins not available right now", "TIME", 1);
	}
        shell=login_getcapstr(lc, "shell", pwd->pw_shell, pwd->pw_shell);
#else /* !LOGIN_CAP */
       shell=pwd->pw_shell;
#endif /* LOGIN_CAP */
	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = _PATH_BSHELL;
	if (*shell == '\0')   /* Not overridden */
		shell = pwd->pw_shell;
	if ((shell = strdup(shell)) == NULL) {
		syslog(LOG_NOTICE, "memory allocation error");
		sleepexit(1);
	}

#ifdef LOGIN_ACCESS
	if (login_access(pwd->pw_name, hostname ? full_hostname : tty) == 0)
		refused("Permission denied", "ACCESS", 1);
#endif /* LOGIN_ACCESS */

	/* Nothing else left to fail -- really log in. */
	memset((void *)&utmp, 0, sizeof(utmp));
	(void)time(&utmp.ut_time);
	(void)strncpy(utmp.ut_name, username, sizeof(utmp.ut_name));
	if (hostname)
		(void)strncpy(utmp.ut_host, hostname, sizeof(utmp.ut_host));
	(void)strncpy(utmp.ut_line, tty, sizeof(utmp.ut_line));
	login(&utmp);

	dolastlog(quietlog);

	/*
	 * Set device protections, depending on what terminal the
	 * user is logged in. This feature is used on Suns to give
	 * console users better privacy.
	 */
	login_fbtab(tty, pwd->pw_uid, pwd->pw_gid);

	(void)chown(ttyn, pwd->pw_uid,
		    (gr = getgrnam(TTYGRPNAME)) ? gr->gr_gid : pwd->pw_gid);

	/*
	 * Preserve TERM if it happens to be already set.
	 */
	if ((term = getenv("TERM")) != NULL)
		term = strdup(term);

	/*
	 * Exclude cons/vt/ptys only, assume dialup otherwise
	 * TODO: Make dialup tty determination a library call
	 * for consistency (finger etc.)
	 */
	if (hostname==NULL && isdialuptty(tty))
		syslog(LOG_INFO, "DIALUP %s, %s", tty, pwd->pw_name);

#ifdef KERBEROS
	if (!quietlog && notickets == 1 && !noticketsdontcomplain)
		(void)printf("Warning: no Kerberos tickets issued.\n");
#endif

#ifdef LOGALL
	/*
	 * Syslog each successful login, so we don't have to watch hundreds
	 * of wtmp or lastlogin files.
	 */
	if (hostname)
		syslog(LOG_INFO, "login from %s on %s as %s",
		       full_hostname, tty, pwd->pw_name);
	else
		syslog(LOG_INFO, "login on %s as %s",
		       tty, pwd->pw_name);
#endif

	/*
	 * If fflag is on, assume caller/authenticator has logged root login.
	 */
	if (rootlogin && fflag == 0)
	{
		if (hostname)
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s FROM %s",
			       username, tty, full_hostname);
		else
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s",
			       username, tty);
	}

	/*
	 * Destroy environment unless user has requested its preservation.
	 * We need to do this before setusercontext() because that may
	 * set or reset some environment variables.
	 */
	if (!pflag)
		environ = envinit;

	/*
	 * We don't need to be root anymore, so
	 * set the user and session context
	 */
#ifdef LOGIN_CAP
	if (setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETALL) != 0) {
                syslog(LOG_ERR, "setusercontext() failed - exiting");
		exit(1);
	}
#else
     	if (setlogin(pwd->pw_name) < 0)
                syslog(LOG_ERR, "setlogin() failure: %m");

	(void)setgid(pwd->pw_gid);
	initgroups(username, pwd->pw_gid);
	(void)setuid(rootlogin ? 0 : pwd->pw_uid);
#endif

	(void)setenv("SHELL", pwd->pw_shell, 1);
	(void)setenv("HOME", pwd->pw_dir, 1);
	if (term != NULL && *term != '\0')
		(void)setenv("TERM", term, 1);	/* Preset overrides */
	else {
		(void)setenv("TERM", stypeof(tty), 0);	/* Fallback doesn't */
	}
	(void)setenv("LOGNAME", pwd->pw_name, 1);
	(void)setenv("USER", pwd->pw_name, 1);
	(void)setenv("PATH", rootlogin ? _PATH_STDPATH : _PATH_DEFPATH, 0);
#ifdef KERBEROS
	if (krbtkfile_env)
		(void)setenv("KRBTKFILE", krbtkfile_env, 1);
#endif
#if LOGIN_CAP_AUTH
	auth_env();
#endif

#ifdef LOGIN_CAP
	if (!quietlog) {
		char	*cw;

		cw = login_getcapstr(lc, "copyright", NULL, NULL);
		if (cw != NULL && access(cw, F_OK) == 0)
			motd(cw);
		else
		    (void)printf("%s\n\t%s %s\n",
	"Copyright (c) 1980, 1983, 1986, 1988, 1990, 1991, 1993, 1994",
	"The Regents of the University of California. ",
	"All rights reserved.");

		(void)printf("\n");

		cw = login_getcapstr(lc, "welcome", NULL, NULL);
		if (cw == NULL || access(cw, F_OK) != 0)
			cw = _PATH_MOTDFILE;
		motd(cw);

		cw = getenv("MAIL");	/* $MAIL may have been set by class */
		if (cw != NULL) {
			strncpy(tbuf, cw, sizeof(tbuf));
			tbuf[sizeof(tbuf)-1] = '\0';
		} else
			snprintf(tbuf, sizeof(tbuf), "%s/%s",
				 _PATH_MAILDIR, pwd->pw_name);
#else
	if (!quietlog) {
		    (void)printf("%s\n\t%s %s\n",
	"Copyright (c) 1980, 1983, 1986, 1988, 1990, 1991, 1993, 1994",
	"The Regents of the University of California. ",
	"All rights reserved.");
		motd(_PATH_MOTDFILE);
		snprintf(tbuf, sizeof(tbuf), "%s/%s",
			 _PATH_MAILDIR, pwd->pw_name);
#endif
		if (stat(tbuf, &st) == 0 && st.st_size != 0)
			(void)printf("You have %smail.\n",
				     (st.st_mtime > st.st_atime) ? "new " : "");
	}

#ifdef LOGIN_CAP
	login_close(lc);
#endif

	(void)signal(SIGALRM, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGTSTP, SIG_IGN);

	if (changepass) {
		if (system(_PATH_CHPASS) != 0)
			sleepexit(1);
	}

	/*
	 * Login shells have a leading '-' in front of argv[0]
	 */
	tbuf[0] = '-';
	(void)strcpy(tbuf + 1, (p = strrchr(pwd->pw_shell, '/')) ? p + 1 : pwd->pw_shell);

	execlp(shell, tbuf, 0);
	err(1, "%s", shell);
}


/*
 * Allow for authentication style and/or kerberos instance
 * */

#define	NBUFSIZ		UT_NAMESIZE + 64

void
getloginname()
{
	int ch;
	char *p;
	static char nbuf[NBUFSIZ];

	for (;;) {
		(void)printf("login: ");
		for (p = nbuf; (ch = getchar()) != '\n'; ) {
			if (ch == EOF) {
				badlogin(username);
				exit(0);
			}
			if (p < nbuf + (NBUFSIZ - 1))
				*p++ = ch;
		}
		if (p > nbuf)
			if (nbuf[0] == '-')
				(void)fprintf(stderr,
				    "login names may not start with '-'.\n");
			else {
				*p = '\0';
				username = nbuf;
				break;
			}
	}
}

int
rootterm(ttyn)
	char *ttyn;
{
	struct ttyent *t;

	return ((t = getttynam(ttyn)) && t->ty_status & TTY_SECURE);
}

volatile int motdinterrupt;

/* ARGSUSED */
void
sigint(signo)
	int signo;
{
	motdinterrupt = 1;
}

void
motd(motdfile)
	char *motdfile;
{
	int fd, nchars;
	sig_t oldint;
	char tbuf[256];

	if ((fd = open(motdfile, O_RDONLY, 0)) < 0)
		return;
	motdinterrupt = 0;
	oldint = signal(SIGINT, sigint);
	while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0 && !motdinterrupt)
		(void)write(fileno(stdout), tbuf, nchars);
	(void)signal(SIGINT, oldint);
	(void)close(fd);
}

/* ARGSUSED */
void
timedout(signo)
	int signo;
{
	(void)fprintf(stderr, "Login timed out after %d seconds\n", timeout);
	exit(0);
}

#ifndef LOGIN_CAP
void
checknologin()
{
	int fd, nchars;
	char tbuf[8192];

	if ((fd = open(_PATH_NOLOGIN, O_RDONLY, 0)) >= 0) {
		while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
			(void)write(fileno(stdout), tbuf, nchars);
		sleepexit(0);
	}
}
#endif

void
dolastlog(quiet)
	int quiet;
{
	struct lastlog ll;
	int fd;

	if ((fd = open(_PATH_LASTLOG, O_RDWR, 0)) >= 0) {
		(void)lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), L_SET);
		if (!quiet) {
			if (read(fd, (char *)&ll, sizeof(ll)) == sizeof(ll) &&
			    ll.ll_time != 0) {
				(void)printf("Last login: %.*s ",
				    24-5, (char *)ctime(&ll.ll_time));
				if (*ll.ll_host != '\0')
					(void)printf("from %.*s\n",
					    (int)sizeof(ll.ll_host),
					    ll.ll_host);
				else
					(void)printf("on %.*s\n",
					    (int)sizeof(ll.ll_line),
					    ll.ll_line);
			}
			(void)lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), L_SET);
		}
		memset((void *)&ll, 0, sizeof(ll));
		(void)time(&ll.ll_time);
		(void)strncpy(ll.ll_line, tty, sizeof(ll.ll_line));
		if (hostname)
			(void)strncpy(ll.ll_host, hostname, sizeof(ll.ll_host));
		(void)write(fd, (char *)&ll, sizeof(ll));
		(void)close(fd);
	}
}

void
badlogin(name)
	char *name;
{

	if (failures == 0)
		return;
	if (hostname) {
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s FROM %s",
		    failures, failures > 1 ? "S" : "", full_hostname);
		syslog(LOG_AUTHPRIV|LOG_NOTICE,
		    "%d LOGIN FAILURE%s FROM %s, %s",
		    failures, failures > 1 ? "S" : "", full_hostname, name);
	} else {
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s ON %s",
		    failures, failures > 1 ? "S" : "", tty);
		syslog(LOG_AUTHPRIV|LOG_NOTICE,
		    "%d LOGIN FAILURE%s ON %s, %s",
		    failures, failures > 1 ? "S" : "", tty, name);
	}
}

#undef	UNKNOWN
#define	UNKNOWN	"su"

char *
stypeof(ttyid)
	char *ttyid;
{

	struct ttyent *t;

	return (ttyid && (t = getttynam(ttyid)) ? t->ty_type : UNKNOWN);
}

void
refused(msg, rtype, lout)
	char *msg;
	char *rtype;
	int lout;
{

	if (msg != NULL)
	    printf("%s.\n", msg);
	if (hostname)
		syslog(LOG_NOTICE, "LOGIN %s REFUSED (%s) FROM %s ON TTY %s",
		       pwd->pw_name, rtype, full_hostname, tty);
	else
		syslog(LOG_NOTICE, "LOGIN %s REFUSED (%s) ON TTY %s",
		       pwd->pw_name, rtype, tty);
	if (lout)
		sleepexit(1);
}

void
sleepexit(eval)
	int eval;
{

	(void)sleep(5);
	exit(eval);
}
