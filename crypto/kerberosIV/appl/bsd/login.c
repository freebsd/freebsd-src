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

/*
 * login [ name ]
 * login -h hostname	(for telnetd, etc.)
 * login -f name	(for pre-authenticated login: datakit, xterm, etc.)
 */

#include "bsd_locl.h"

RCSID("$Id: login.c,v 1.104 1997/05/20 20:35:06 assar Exp $");

#include <otp.h>

#include "sysv_default.h"
#ifdef SYSV_SHADOW
#include "sysv_shadow.h"
#endif

static	void	 badlogin (char *);
static	void	 checknologin (void);
static	void	 dolastlog (int);
static	void	 getloginname (int);
static	int	 rootterm (char *);
static	char	*stypeof (char *);
static	RETSIGTYPE	 timedout (int);
static	int	 doremotelogin (char *);
void	login_fbtab (char *, uid_t, gid_t);
#ifdef KERBEROS
int	klogin (struct passwd *, char *, char *, char *);
#endif

#define	TTYGRPNAME	"tty"		/* name of group to own ttys */

/*
 * This bounds the time given to login.  Change it in
 * `/etc/default/login'.
 */

static	u_int	login_timeout;

#ifdef KERBEROS
int	notickets = 1;
int	noticketsdontcomplain = 1;
char	*instance;
char	*krbtkfile_env;
int	authok;
#endif

#ifdef HAVE_SHADOW_H
static  struct spwd *spwd = NULL;
#endif

static	char    *ttyprompt;

static	struct	passwd *pwd;
static	int	failures;
static	char	term[64], *hostname, *username, *tty;

static  char rusername[100], lusername[100];

static int
change_passwd(struct passwd  *who)
{
        int             status;
        int             pid;
        int             wpid;
 
        switch (pid = fork()) {
	case -1:
	    warn("fork /bin/passwd");
	    sleepexit(1);
	case 0:
	    execlp("/bin/passwd", "passwd", who->pw_name, (char *) 0);
	    _exit(1);
	default:
	    while ((wpid = wait(&status)) != -1 && wpid != pid)
		/* void */ ;
	    return (status);
	}
}

#ifndef NO_MOTD /* message of the day stuff */

jmp_buf motdinterrupt;

static RETSIGTYPE
sigint(int signo)
{
	longjmp(motdinterrupt, 1);
}

static void
motd(void)
{
	int fd, nchars;
	RETSIGTYPE (*oldint)();
	char tbuf[8192];

	if ((fd = open(_PATH_MOTDFILE, O_RDONLY, 0)) < 0)
		return;
	oldint = signal(SIGINT, sigint);
	if (setjmp(motdinterrupt) == 0)
		while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
			write(fileno(stdout), tbuf, nchars);
	signal(SIGINT, oldint);
	close(fd);
}

#endif /* !NO_MOTD */

#define AUTH_NONE 0
#define AUTH_OTP  1

/*
 * getpwnam and try to detect the worst form of NIS attack.
 */

static struct passwd *
paranoid_getpwnam (char *user)
{
	struct passwd *p;

	p = k_getpwnam (user);
	if (p == NULL)
		return p;
	if (p->pw_uid == 0 && strcmp (username, "root") != 0) {
		syslog (LOG_ALERT,
			"NIS attack, user %s has uid 0", username);
		return NULL;
	}
	return p;
}

int
main(int argc, char **argv)
{
	struct group *gr;
	int ask, ch, cnt, fflag, hflag, pflag, quietlog, nomailcheck;
	int rootlogin, rval;
	int rflag;
	int changepass = 0;
	uid_t uid;
	char *domain, *p, passwd[128], *ttyn;
	char tbuf[MaxPathLen + 2], tname[sizeof(_PATH_TTY) + 10];
	char localhost[MaxHostNameLen];
	char full_hostname[MaxHostNameLen];
	int auth_level = AUTH_NONE;
	OtpContext otp_ctx;
	int mask = 022;		/* Default umask (set below) */
	int maxtrys = 5;	/* Default number of allowed failed logins */

	set_progname(argv[0]);

	openlog("login", LOG_ODELAY, LOG_AUTH);

        /* Read defaults file and set the login timeout period. */
        sysv_defaults();
        login_timeout = atoi(default_timeout);
        maxtrys = atoi(default_maxtrys);
        if (sscanf(default_umask, "%o", &mask) != 1 || (mask & ~0777))
                syslog(LOG_WARNING, "bad umask default: %s", default_umask);
        else
                umask(mask);

	signal(SIGALRM, timedout);
	alarm(login_timeout);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	setpriority(PRIO_PROCESS, 0, 0);

	/*
	 * -p is used by getty to tell login not to destroy the environment
	 * -f is used to skip a second login authentication
	 * -h is used by other servers to pass the name of the remote
	 *    host to login so that it may be placed in utmp and wtmp
	 * -r is used by old-style rlogind to execute the autologin protocol
	 */

	*full_hostname = '\0';
	domain = NULL;
	if (k_gethostname(localhost, sizeof(localhost)) < 0)
		syslog(LOG_ERR, "couldn't get local hostname: %m");
	else
		domain = strchr(localhost, '.');

	fflag = hflag = pflag = rflag = 0;
	uid = getuid();
	while ((ch = getopt(argc, argv, "a:d:fh:pr:")) != EOF)
	        switch (ch) {
		case 'a':
			if (strcmp (optarg, "none") == 0)
				auth_level = AUTH_NONE;
			else if (strcmp (optarg, "otp") == 0)
				auth_level = AUTH_OTP;
			else
				warnx ("bad value for -a: %s", optarg);
			break;
		case 'd':
			break;
		case 'f':
			fflag = 1;
			break;
		case 'h':
			if (rflag || hflag) {
			    printf("Only one of -r and -h allowed\n");
			    exit(1);
                        }
			if (uid)
				errx(1, "-h option: %s", strerror(EPERM));
			hflag = 1;
			strncpy(full_hostname, optarg, sizeof(full_hostname)-1);
			if (domain && (p = strchr(optarg, '.')) &&
			    strcasecmp(p, domain) == 0)
				*p = 0;
			hostname = optarg;
			break;
		case 'p':
			if (getuid()) {
				warnx("-p for super-user only.");
				exit(1);
                        }
			pflag = 1;
			break;
	        case 'r':
			if (rflag || hflag) {
				warnx("Only one of -r and -h allowed\n");
				exit(1);
                        }
			if (getuid()) {
				warnx("-r for super-user only.");
				exit(1);
                        }
			rflag = 1;
			strncpy(full_hostname, optarg, sizeof(full_hostname)-1);
			if (domain && (p = strchr(optarg, '.')) &&
			    strcasecmp(p, domain) == 0)
				*p = 0;
			hostname = optarg;
			fflag = (doremotelogin(full_hostname) == 0);
			break;
		case '?':
		default:
			if (!uid)
				syslog(LOG_ERR, "invalid flag %c", ch);
			fprintf(stderr,
				"usage: login [-fp] [-a otp]"
				"[-h hostname | -r hostname] [username]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (geteuid() != 0) {
		warnx("only root may use login, use su");
	        /* Or install login setuid root, which is not necessary */
		sleep(10);
		exit(1);
	}
        /*
         * Figure out if we should ask for the username or not. The name
         * may be given on the command line or via the environment, and
         * it may even be in the terminal input queue.
         */
        if (rflag) {
                username = lusername;
                ask = 0;
	      } else
        if (*argv && strchr(*argv, '=')) {
                ask = 1;
	      } else
        if (*argv && strcmp(*argv, "-") == 0) {
                argc--;
                argv++;
                ask = 1;
	      } else
	if (*argv) {
		username = *argv;
		ask = 0;
                argc--;
                argv++;
        } else if ((ttyprompt = getenv("TTYPROMPT")) && *ttyprompt) {
                getloginname(0);
                ask = 0;
	} else
		ask = 1;

	/* Default tty settings. */
	stty_default();

	for (cnt = getdtablesize(); cnt > 2; cnt--)
		close(cnt);

        /*
         * Determine the tty name. BSD takes the basename, SYSV4 takes
         * whatever remains after stripping the "/dev/" prefix. The code
         * below should produce sensible results in either environment.
         */
        ttyn = ttyname(STDIN_FILENO);
        if (ttyn == NULL || *ttyn == '\0') {
	        snprintf(tname, sizeof(tname), "%s??", _PATH_TTY);
	        ttyn = tname;
	}
        if ((tty = strchr(ttyn + 1, '/')))
	        ++tty;
        else
	        tty = ttyn;

	for (cnt = 0;; ask = 1) {
	        char prompt[128], ss[256];
		if (ask) {
			fflag = 0;
			getloginname(1);
		}
		rootlogin = 0;
		rval = 1;
#ifdef	KERBEROS
		if ((instance = strchr(username, '.')) != NULL) {
		    if (strcmp(instance, ".root") == 0)
			rootlogin = 1;
		    *instance++ = '\0';
		} else
		    instance = "";
#endif
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
		strcpy(tbuf, username);

		pwd = paranoid_getpwnam (username);

		/*
		 * if we have a valid account name, and it doesn't have a
		 * password, or the -f option was specified and the caller
		 * is root or the caller isn't changing their uid, don't
		 * authenticate.
		 */
		if (pwd) {
			if (pwd->pw_uid == 0)
				rootlogin = 1;

			if (fflag && (uid == 0 || uid == pwd->pw_uid)) {
				/* already authenticated */
				break;
			} else if (pwd->pw_passwd[0] == '\0') {
				/* pretend password okay */
				rval = 0;
				goto ttycheck;
			}
		}

		fflag = 0;

		setpriority(PRIO_PROCESS, 0, -4);

		if (otp_challenge (&otp_ctx, username,
				   ss, sizeof(ss)) == 0)
			snprintf (prompt, sizeof(prompt), "%s's %s Password: ",
				  username, ss);
		else {
			if (auth_level == AUTH_NONE)
				snprintf(prompt, sizeof(prompt), "%s's Password: ",
					 username);
			else {
				char *s;

				rval = 1;
				s = otp_error(&otp_ctx);
				if(s)
					printf ("OTP: %s\n", s);
				continue;
			}
		}

		if (des_read_pw_string (passwd, sizeof(passwd) - 1, prompt, 0))
			continue;
		passwd[sizeof(passwd) - 1] = '\0';

		/* Verify it somehow */

		if (otp_verify_user (&otp_ctx, passwd) == 0)
			rval = 0;
		else if (pwd == NULL)
			;
		else if (auth_level == AUTH_NONE) {
			uid_t pwd_uid = pwd->pw_uid;

			rval = unix_verify_user (username, passwd);

			if (rval == 0)
			  {
			    if (rootlogin && pwd_uid != 0)
			      rootlogin = 0;
			  }
			else
			  {
			    rval = klogin(pwd, instance, localhost, passwd);
			    if (rval != 0 && rootlogin && pwd_uid != 0)
			      rootlogin = 0;
			    if (rval == 0)
			      authok = 1;
			  }
		} else {
			char *s;

			rval = 1;
			if ((s = otp_error(&otp_ctx)))
				printf ("OTP: %s\n", s);
		}

		memset (passwd, 0, sizeof(passwd));
		setpriority (PRIO_PROCESS, 0, 0);

		/*
		 * Santa Claus, give me a portable and reentrant getpwnam.
		 */
		pwd = paranoid_getpwnam (username);

	ttycheck:
		/*
		 * If trying to log in as root without Kerberos,
		 * but with insecure terminal, refuse the login attempt.
		 */
#ifdef KERBEROS
		if (authok == 0)
#endif
		if (pwd && !rval && rootlogin && !rootterm(tty)
		    && !rootterm(ttyn)) {
			warnx("%s login refused on this terminal.",
			      pwd->pw_name);
			if (hostname)
				syslog(LOG_NOTICE,
				       "LOGIN %s REFUSED FROM %s ON TTY %s",
				       pwd->pw_name, hostname, tty);
			else
				syslog(LOG_NOTICE,
				       "LOGIN %s REFUSED ON TTY %s",
				       pwd->pw_name, tty);
			continue;
		}

		if (rval == 0)
			break;

		printf("Login incorrect\n");
		failures++;

                /* max number of attemps and delays taken from defaults file */
		/* we allow maxtrys tries, but after 2 we start backing off */
		if (++cnt > 2) {
			if (cnt >= maxtrys) {
				badlogin(username);
				sleepexit(1);
			}
			sleep((u_int)((cnt - 2) * atoi(default_sleep)));
		}
	}

	/* committed to login -- turn off timeout */
	alarm(0);

	endpwent();

#if defined(HAVE_GETUDBNAM) && defined(HAVE_SETLIM)
	{
	    struct udb *udb;
	    long t;
	    const long maxcpu = 46116860184; /* some random constant */
	    udb = getudbnam(pwd->pw_name);
	    if(udb == UDB_NULL){
		    warnx("Failed to get UDB entry.");
		    exit(1);
	    }
	    t = udb->ue_pcpulim[UDBRC_INTER];
	    if(t == 0 || t > maxcpu)
		t = CPUUNLIM;
	    else
		t *= 100 * CLOCKS_PER_SEC;

	    if(limit(C_PROC, 0, L_CPU, t) < 0)
		warn("limit C_PROC");

	    t = udb->ue_jcpulim[UDBRC_INTER];
	    if(t == 0 || t > maxcpu)
		t = CPUUNLIM;
	    else
		t *= 100 * CLOCKS_PER_SEC;

	    if(limit(C_JOBPROCS, 0, L_CPU, t) < 0)
		warn("limit C_JOBPROCS");

	    nice(udb->ue_nice[UDBRC_INTER]);
	}
#endif
	/* if user not super-user, check for disabled logins */
	if (!rootlogin)
		checknologin();

	if (chdir(pwd->pw_dir) < 0) {
		printf("No home directory %s!\n", pwd->pw_dir);
		if (chdir("/"))
			exit(0);
		pwd->pw_dir = "/";
		printf("Logging in with home = \"/\".\n");
	}

	quietlog = access(_PATH_HUSHLOGIN, F_OK) == 0;
	nomailcheck = access(_PATH_NOMAILCHECK, F_OK) == 0;

#if defined(HAVE_PASSWD_CHANGE) && defined(HAVE_PASSWD_EXPIRE)
	if (pwd->pw_change || pwd->pw_expire)
		gettimeofday(&tp, (struct timezone *)NULL);

	if (pwd->pw_change)
		if (tp.tv_sec >= pwd->pw_change) {
			printf("Sorry -- your password has expired.\n");
			changepass=1;
		} else if (pwd->pw_change - tp.tv_sec <
		    2 * DAYSPERWEEK * SECSPERDAY && !quietlog)
			printf("Warning: your password expires on %s",
			    ctime(&pwd->pw_change));
	if (pwd->pw_expire)
		if (tp.tv_sec >= pwd->pw_expire) {
			printf("Sorry -- your account has expired.\n");
			sleepexit(1);
		} else if (pwd->pw_expire - tp.tv_sec <
		    2 * DAYSPERWEEK * SECSPERDAY && !quietlog)
			printf("Warning: your account expires on %s",
			    ctime(&pwd->pw_expire));
#endif /* defined(HAVE_PASSWD_CHANGE) && defined(HAVE_PASSWD_EXPIRE) */

	/* Nothing else left to fail -- really log in. */

        /*
         * Update the utmp files, both BSD and SYSV style.
         */
        if (utmpx_login(tty, username, hostname ? hostname : "") != 0
	    && !fflag) {
                printf("No utmpx entry.  You must exec \"login\" from the lowest level \"sh\".\n");
                sleepexit(0);
        }
	utmp_login(ttyn, username, hostname ? hostname : "");
	dolastlog(quietlog);

	/*
	 * Set device protections, depending on what terminal the
	 * user is logged in. This feature is used on Suns to give
	 * console users better privacy.
	 */
	login_fbtab(tty, pwd->pw_uid, pwd->pw_gid);

	chown(ttyn, pwd->pw_uid,
	    (gr = getgrnam(TTYGRPNAME)) ? gr->gr_gid : pwd->pw_gid);
	chmod(ttyn, S_IRUSR | S_IWUSR | S_IWGRP);
	setgid(pwd->pw_gid);

	initgroups(username, pwd->pw_gid);

	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = _PATH_BSHELL;

        /*
         * Set up a new environment. With SYSV, some variables are always
         * preserved; some varables are never preserved, and some variables
         * are always clobbered. With BSD, nothing is always preserved, and
         * some variables are always clobbered. We add code to make sure
         * that LD_* and IFS are never preserved.
         */
	if (term[0] == '\0')
		strncpy(term, stypeof(tty), sizeof(term));
        /* set up a somewhat censored environment. */
        sysv_newenv(argc, argv, pwd, term, pflag);
#ifdef KERBEROS
	if (krbtkfile_env)
	    setenv("KRBTKFILE", krbtkfile_env, 1);
#endif

	if (tty[sizeof("tty")-1] == 'd')
		syslog(LOG_INFO, "DIALUP %s, %s", tty, pwd->pw_name);

	/* If fflag is on, assume caller/authenticator has logged root login. */
	if (rootlogin && fflag == 0)
		if (hostname)
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s FROM %s",
			    username, tty, hostname);
		else
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s", username, tty);

#ifdef KERBEROS
	if (!quietlog && notickets == 1 && !noticketsdontcomplain)
		printf("Warning: no Kerberos tickets issued.\n");
#endif

#ifdef LOGALL
	/*
	 * Syslog each successful login, so we don't have to watch hundreds
	 * of wtmp or lastlogin files.
	 */
	if (hostname) {
		syslog(LOG_INFO, "login from %s as %s", hostname, pwd->pw_name);
	} else {
		syslog(LOG_INFO, "login on %s as %s", tty, pwd->pw_name);
	}
#endif

#ifndef NO_MOTD
        /*
         * Optionally show the message of the day. System V login leaves
         * motd and mail stuff up to the shell startup file.
         */
	if (!quietlog) {
	        struct stat st;
#if 0
		printf("%s\n\t%s  %s\n\n",
	    "Copyright (c) 1980, 1983, 1986, 1988, 1990, 1991, 1993, 1994",
		    "The Regents of the University of California. ",
		    "All rights reserved.");
#endif
		motd();
		if(!nomailcheck){
		    snprintf(tbuf, sizeof(tbuf), "%s/%s", _PATH_MAILDIR, pwd->pw_name);
		    if (stat(tbuf, &st) == 0 && st.st_size != 0)
			printf("You have %smail.\n",
			       (st.st_mtime > st.st_atime) ? "new " : "");
		}
	}
#endif /* NO_MOTD */

#ifdef LOGIN_ACCESS
	if (login_access(pwd->pw_name, hostname ? full_hostname : tty) == 0) {
		printf("Permission denied\n");
		if (hostname)
			syslog(LOG_NOTICE, "%s LOGIN REFUSED FROM %s",
				pwd->pw_name, hostname);
		else
			syslog(LOG_NOTICE, "%s LOGIN REFUSED ON %s",
				pwd->pw_name, tty);
		sleepexit(1);
	}
#endif

	signal(SIGALRM, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGTSTP, SIG_IGN);

	tbuf[0] = '-';
	strcpy(tbuf + 1, (p = strrchr(pwd->pw_shell, '/')) ?
	       p + 1 : pwd->pw_shell);

#ifdef HAVE_SETLOGIN
     	if (setlogin(pwd->pw_name) < 0)
                syslog(LOG_ERR, "setlogin() failure: %m");
#endif

#ifdef HAVE_SETPCRED
	if (setpcred (pwd->pw_name, NULL) == -1)
		syslog(LOG_ERR, "setpcred() failure: %m");
#endif /* HAVE_SETPCRED */

#if defined(SYSV_SHADOW) && defined(HAVE_GETSPNAM)
	spwd = getspnam (username);
	endspent ();
#endif
	/* Discard permissions last so can't get killed and drop core. */
	{
	    int uid = rootlogin ? 0 : pwd->pw_uid;
	    if(setuid(uid) != 0){
		    warn("setuid(%d)", uid);
		    if(!rootlogin)
			    exit(1);
	    }
	}
		       

	/*
         * After dropping privileges and after cleaning up the environment,
         * optionally run, as the user, /bin/passwd.
         */
 
        if (pwd->pw_passwd[0] == 0 &&
	    strcasecmp(default_passreq, "YES") == 0) {
                printf("You don't have a password.  Choose one.\n");
                if (change_passwd(pwd))
                        sleepexit(0);
		changepass = 0;
        }

#ifdef SYSV_SHADOW
        if (spwd && sysv_expire(spwd)) {
                if (change_passwd(pwd))
                        sleepexit(0);
		changepass = 0;
        }
#endif /* SYSV_SHADOW */
	if (changepass) {
		int res;
		if ((res=system(_PATH_CHPASS)))
			sleepexit(1);
	}

	if (k_hasafs()) {
	    char cell[64];
	    k_setpag();
	    if(k_afs_cell_of_file(pwd->pw_dir, cell, sizeof(cell)) == 0)
		k_afsklog(cell, 0);
	    k_afsklog(0, 0);
	}

	execlp(pwd->pw_shell, tbuf, 0);
	if (getuid() == 0) {
		warnx("Can't exec %s, trying %s\n", 
		      pwd->pw_shell, _PATH_BSHELL);
		execlp(_PATH_BSHELL, tbuf, 0);
		err(1, "%s", _PATH_BSHELL);
	}
	err(1, "%s", pwd->pw_shell);
	return 1;
}

#ifdef	KERBEROS
#define	NBUFSIZ		(UT_NAMESIZE + 1 + 5)	/* .root suffix */
#else
#define	NBUFSIZ		(UT_NAMESIZE + 1)
#endif

static void
getloginname(int prompt)
{
	int ch;
	char *p;
	static char nbuf[NBUFSIZ];

	for (;;) {
                if (prompt)
                    if (ttyprompt && *ttyprompt)
                        printf("%s", ttyprompt);
                    else
		        printf("login: ");
		prompt = 1;
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
				warnx("login names may not start with '-'.");
			else {
				*p = '\0';
				username = nbuf;
				break;
			}
	}
}

static int
rootterm(char *ttyn)
{
#ifndef HAVE_TTYENT_H
        return (default_console == 0 || strcmp(default_console, ttyname(0)) == 0);
#else
	struct ttyent *t;

	return ((t = getttynam(ttyn)) && t->ty_status & TTY_SECURE);
#endif
}

static RETSIGTYPE
timedout(int signo)
{
	fprintf(stderr, "Login timed out after %d seconds\n",
		login_timeout);
	exit(0);
}

static void
checknologin(void)
{
	int fd, nchars;
	char tbuf[8192];

	if ((fd = open(_PATH_NOLOGIN, O_RDONLY, 0)) >= 0) {
		while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
			write(fileno(stdout), tbuf, nchars);
		sleepexit(0);
	}
}

static void
dolastlog(int quiet)
{
#if defined(HAVE_LASTLOG_H) || defined(HAVE_LOGIN_H) || defined(SYSV_SHADOW)
	struct lastlog ll;
	int fd;

	if ((fd = open(_PATH_LASTLOG, O_RDWR, 0)) >= 0) {
		lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), SEEK_SET);
#ifdef SYSV_SHADOW
                if (read(fd, &ll, sizeof(ll)) == sizeof(ll) &&
                    ll.ll_time != 0) {
                        if (pwd->pw_uid && spwd && spwd->sp_inact > 0
                            && ll.ll_time / (24 * 60 * 60)
			    + spwd->sp_inact < time(0)) {
                                printf("Your account has been inactive too long.\n");
                                sleepexit(1);
                        }
                        if (!quiet) {
                                printf("Last login: %.*s ",
                                    24-5, ctime(&ll.ll_time));
                                if (*ll.ll_host != '\0') {
                                        printf("from %.*s\n",
                                            (int)sizeof(ll.ll_host),
					       ll.ll_host);
                                } else
                                        printf("on %.*s\n",
                                            (int)sizeof(ll.ll_line),
					       ll.ll_line);
                        }
                }
                lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), SEEK_SET);
#else /* SYSV_SHADOW */
		if (!quiet) {
			if (read(fd, &ll, sizeof(ll)) == sizeof(ll) &&
			    ll.ll_time != 0) {
				printf("Last login: %.*s ",
				    24-5, ctime(&ll.ll_time));
				if (*ll.ll_host != '\0')
					printf("from %.*s\n",
					    (int)sizeof(ll.ll_host),
					    ll.ll_host);
				else
					printf("on %.*s\n",
					    (int)sizeof(ll.ll_line),
					    ll.ll_line);
			}
			lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), SEEK_SET);
		}
#endif /* SYSV_SHADOW */
		memset(&ll, 0, sizeof(ll));
		time(&ll.ll_time);
		strncpy(ll.ll_line, tty, sizeof(ll.ll_line));
		if (hostname)
			strncpy(ll.ll_host, hostname, sizeof(ll.ll_host));
		write(fd, &ll, sizeof(ll));
		close(fd);
	}
#endif /* DOLASTLOG */
}

static void
badlogin(char *name)
{

	if (failures == 0)
		return;
	if (hostname) {
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
}

#undef	UNKNOWN
#define	UNKNOWN	"su"

static char *
stypeof(char *ttyid)
{
    /* TERM is probably a better guess than anything else. */
    char *term = getenv("TERM");

    if (term != 0 && term[0] != 0)
	return term;

    {
#ifndef HAVE_TTYENT_H
	return UNKNOWN;
#else
	struct ttyent *t;
	return (ttyid && (t = getttynam(ttyid)) ? t->ty_type : UNKNOWN);
#endif
    }
}

static void
xgetstr(char *buf, int cnt, char *err)
{
        char ch;
 
        do {
                if (read(0, &ch, sizeof(ch)) != sizeof(ch))
                        exit(1);
                if (--cnt < 0) {
                        fprintf(stderr, "%s too long\r\n", err);
                        sleepexit(1);
                }
                *buf++ = ch;
        } while (ch);
}

/*
 * Some old rlogind's unknowingly pass remuser, locuser and
 * terminal_type/speed so we need to take care of that part of the
 * protocol here. Also, we can't make a getpeername(2) on the socket
 * so we have to trust that rlogind resolved the name correctly.
 */

static int
doremotelogin(char *host)
{
        int code;
        char *cp;

        xgetstr(rusername, sizeof (rusername), "remuser");
        xgetstr(lusername, sizeof (lusername), "locuser");
        xgetstr(term, sizeof(term), "Terminal type");
	cp = strchr(term, '/');
	if (cp != 0)
		*cp = 0;	/* For now ignore speed/bg */
        pwd = k_getpwnam(lusername);
        if (pwd == NULL)
                return(-1);
        code = ruserok(host, (pwd->pw_uid == 0), rusername, lusername);
	if (code == 0)
	  syslog(LOG_NOTICE,
		 "Warning: An old rlogind accepted login probably from host %s",
		 host);
	return(code);
}
 
void
sleepexit(int eval)
{

	sleep(5);
	exit(eval);
}
