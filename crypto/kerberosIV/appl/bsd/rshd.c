/*-
 * Copyright (c) 1988, 1989, 1992, 1993, 1994
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
 * remote shell server:
 *	[port]\0
 *	remuser\0
 *	locuser\0
 *	command\0
 *	data
 */

#include "bsd_locl.h"

RCSID("$Id: rshd.c,v 1.60.2.3 2000/10/18 20:39:12 assar Exp $");

extern char *__rcmd_errstr; /* syslog hook from libc/net/rcmd.c. */
extern int __check_rhosts_file;

static int	keepalive = 1;
static int	log_success;	/* If TRUE, log all successful accesses */
static int	new_pag = 1;	/* Put process in new PAG by default */
static int	no_inetd = 0;
static int	sent_null;

static void		 doit (struct sockaddr_in *);
static void		 error (const char *, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 1, 2)))
#endif
;
static void		 usage (void);

#define	VERSION_SIZE	9
#define SECURE_MESSAGE  "This rsh session is using DES encryption for all transmissions.\r\n"
#define	OPTIONS		"alnkvxLp:Pi"
AUTH_DAT authbuf;
KTEXT_ST tickbuf;
int	doencrypt, use_kerberos, vacuous;
Key_schedule	schedule;

int
main(int argc, char *argv[])
{
    struct linger linger;
    int ch, on = 1, fromlen;
    struct sockaddr_in from;
    int portnum = 0;

    set_progname(argv[0]);

    openlog("rshd", LOG_PID | LOG_ODELAY, LOG_DAEMON);

    opterr = 0;
    while ((ch = getopt(argc, argv, OPTIONS)) != -1)
	switch (ch) {
	case 'a':
	    break;
	case 'l':
	    __check_rhosts_file = 0;
	    break;
	case 'n':
	    keepalive = 0;
	    break;
	case 'k':
	    use_kerberos = 1;
	    break;

	case 'v':
	    vacuous = 1;
	    break;

	case 'x':
	    doencrypt = 1;
	    break;
	case 'L':
	    log_success = 1;
	    break;
	case 'p':
	    portnum = htons(atoi(optarg));
	    break;
	case 'P':
	    new_pag = 0;
	    break;
	case 'i':
	    no_inetd = 1;
	    break;
	case '?':
	default:
	    usage();
	    break;
	}

    argc -= optind;
    argv += optind;

    if (use_kerberos && vacuous) {
	syslog(LOG_ERR, "only one of -k and -v allowed");
	exit(2);
    }
    if (doencrypt && !use_kerberos) {
	syslog(LOG_ERR, "-k is required for -x");
	exit(2);
    }

    if (no_inetd) {
	if(portnum == 0)
	    portnum = get_shell_port (use_kerberos, doencrypt);
	mini_inetd (portnum);
    }

    fromlen = sizeof (from);
    if (getpeername(0, (struct sockaddr *)&from, &fromlen) < 0) {
	syslog(LOG_ERR, "getpeername: %m");
	_exit(1);
    }
#ifdef HAVE_SETSOCKOPT
#ifdef SO_KEEPALIVE
    if (keepalive &&
	setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, (void *)&on,
		   sizeof(on)) < 0)
	syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
#endif
#ifdef SO_LINGER
    linger.l_onoff = 1;
    linger.l_linger = 60;			/* XXX */
    if (setsockopt(0, SOL_SOCKET, SO_LINGER, (void *)&linger,
		   sizeof (linger)) < 0)
	syslog(LOG_WARNING, "setsockopt (SO_LINGER): %m");
#endif
#endif /* HAVE_SETSOCKOPT */
    doit(&from);
    /* NOTREACHED */
    return 0;
}

char	username[20] = "USER=";
char	homedir[64] = "HOME=";
char	shell[64] = "SHELL=";
char	path[100] = "PATH=";
char	*envinit[] =
{homedir, shell, path, username, 0};

static void
xgetstr(char *buf, int cnt, char *err)
{
    char c;

    do {
	if (read(STDIN_FILENO, &c, 1) != 1)
	    exit(1);
	*buf++ = c;
	if (--cnt == 0) {
	    error("%s too long\n", err);
	    exit(1);
	}
    } while (c != 0);
}

static void
doit(struct sockaddr_in *fromp)
{
    struct passwd *pwd;
    u_short port;
    fd_set ready, readfrom;
    int cc, nfd, pv[2], pid, s;
    int one = 1;
    const char *errorhost = "";
    char *errorstr;
    char *cp, sig, buf[DES_RW_MAXWRITE];
    char cmdbuf[NCARGS+1], locuser[16], remuser[16];
    char remotehost[2 * MaxHostNameLen + 1];
    uid_t uid;
    char shell_path[MAXPATHLEN];

    AUTH_DAT	*kdata;
    KTEXT		ticket;
    char		instance[INST_SZ], version[VERSION_SIZE];
    struct		sockaddr_in	fromaddr;
    int		rc;
    long		authopts;
    int		pv1[2], pv2[2];
    fd_set		wready, writeto;

    fromaddr = *fromp;

    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
#ifdef DEBUG
    { int t = open(_PATH_TTY, 2);
    if (t >= 0) {
	ioctl(t, TIOCNOTTY, (char *)0);
	close(t);
    }
    }
#endif
    fromp->sin_port = ntohs((u_short)fromp->sin_port);
    if (fromp->sin_family != AF_INET) {
	syslog(LOG_ERR, "malformed \"from\" address (af %d)\n",
	       fromp->sin_family);
	exit(1);
    }


    if (!use_kerberos) {
	ip_options_and_die (0, fromp);
	if (fromp->sin_port >= IPPORT_RESERVED ||
	    fromp->sin_port < IPPORT_RESERVED/2) {
	    syslog(LOG_NOTICE|LOG_AUTH,
		   "Connection from %s on illegal port %u",
		   inet_ntoa(fromp->sin_addr),
		   fromp->sin_port);
	    exit(1);
	}
    }

    alarm(60);
    port = 0;
    for (;;) {
	char c;
	if ((cc = read(STDIN_FILENO, &c, 1)) != 1) {
	    if (cc < 0)
		syslog(LOG_NOTICE, "read: %m");
	    shutdown(0, 1+1);
	    exit(1);
	}
	if (c== 0)
	    break;
	port = port * 10 + c - '0';
    }

    alarm(0);
    if (port != 0) {
	int lport = IPPORT_RESERVED - 1;
	s = rresvport(&lport);
	if (s < 0) {
	    syslog(LOG_ERR, "can't get stderr port: %m");
	    exit(1);
	}
	if (!use_kerberos)
	    if (port >= IPPORT_RESERVED) {
		syslog(LOG_ERR, "2nd port not reserved\n");
		exit(1);
	    }
	fromp->sin_port = htons(port);
	if (connect(s, (struct sockaddr *)fromp, sizeof (*fromp)) < 0) {
	    syslog(LOG_INFO, "connect second port %d: %m", port);
	    exit(1);
	}
    }

    if (vacuous) {
	error("rshd: Remote host requires Kerberos authentication.\n");
	exit(1);
    }

    errorstr = NULL;
    inaddr2str (fromp->sin_addr, remotehost, sizeof(remotehost));

    if (use_kerberos) {
	kdata = &authbuf;
	ticket = &tickbuf;
	authopts = 0L;
	k_getsockinst(0, instance, sizeof(instance));
	version[VERSION_SIZE - 1] = '\0';
	if (doencrypt) {
	    struct sockaddr_in local_addr;
	    rc = sizeof(local_addr);
	    if (getsockname(0, (struct sockaddr *)&local_addr,
			    &rc) < 0) {
		syslog(LOG_ERR, "getsockname: %m");
		error("rshd: getsockname: %m");
		exit(1);
	    }
	    authopts = KOPT_DO_MUTUAL;
	    rc = krb_recvauth(authopts, 0, ticket,
			      "rcmd", instance, &fromaddr,
			      &local_addr, kdata, "", schedule,
			      version);
#ifndef NOENCRYPTION
	    des_set_key(&kdata->session, schedule);
#else
	    memset(schedule, 0, sizeof(schedule));
#endif
	} else
	    rc = krb_recvauth(authopts, 0, ticket, "rcmd",
			      instance, &fromaddr,
			      (struct sockaddr_in *) 0,
			      kdata, "", 0, version);
	if (rc != KSUCCESS) {
	    error("Kerberos authentication failure: %s\n",
		  krb_get_err_text(rc));
	    exit(1);
	}
    } else
	xgetstr(remuser, sizeof(remuser), "remuser");

    xgetstr(locuser, sizeof(locuser), "locuser");
    xgetstr(cmdbuf, sizeof(cmdbuf), "command");
    setpwent();
    pwd = k_getpwnam(locuser);
    if (pwd == NULL) {
	syslog(LOG_INFO|LOG_AUTH,
	       "%s@%s as %s: unknown login. cmd='%.80s'",
	       remuser, remotehost, locuser, cmdbuf);
	if (errorstr == NULL)
	    errorstr = "Login incorrect.\n";
	goto fail;
    }
    if (pwd->pw_uid == 0 && strcmp("root", locuser) != 0)
	{
	    syslog(LOG_ALERT, "NIS attack, user %s has uid 0", locuser);
	    if (errorstr == NULL)
		errorstr = "Login incorrect.\n";
	    goto fail;
	}
    if (chdir(pwd->pw_dir) < 0) {
	chdir("/");
#ifdef notdef
	syslog(LOG_INFO|LOG_AUTH,
	       "%s@%s as %s: no home directory. cmd='%.80s'",
	       remuser, remotehost, locuser, cmdbuf);
	error("No remote directory.\n");
	exit(1);
#endif
    }

    if (use_kerberos) {
	if (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0') {
	    if (kuserok(kdata, locuser) != 0) {
		syslog(LOG_INFO|LOG_AUTH,
		       "Kerberos rsh denied to %s",
		       krb_unparse_name_long(kdata->pname, 
					     kdata->pinst, 
					     kdata->prealm));
		error("Permission denied.\n");
		exit(1);
	    }
	}
    } else

	if (errorstr ||
	    (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0' &&
	    iruserok(fromp->sin_addr.s_addr, pwd->pw_uid == 0,
		     remuser, locuser) < 0)) {
	    if (__rcmd_errstr)
		syslog(LOG_INFO|LOG_AUTH,
		       "%s@%s as %s: permission denied (%s). cmd='%.80s'",
		       remuser, remotehost, locuser,
		       __rcmd_errstr, cmdbuf);
	    else
		syslog(LOG_INFO|LOG_AUTH,
		       "%s@%s as %s: permission denied. cmd='%.80s'",
		       remuser, remotehost, locuser, cmdbuf);
		     fail:
	    if (errorstr == NULL)
		errorstr = "Permission denied.\n";
	    error(errorstr, errorhost);
	    exit(1);
	}

    if (pwd->pw_uid && !access(_PATH_NOLOGIN, F_OK)) {
	error("Logins currently disabled.\n");
	exit(1);
    }

    write(STDERR_FILENO, "\0", 1);
    sent_null = 1;

    if (port) {
	if (pipe(pv) < 0) {
	    error("Can't make pipe.\n");
	    exit(1);
	}
	if (doencrypt) {
	    if (pipe(pv1) < 0) {
		error("Can't make 2nd pipe.\n");
		exit(1);
	    }
	    if (pipe(pv2) < 0) {
		error("Can't make 3rd pipe.\n");
		exit(1);
	    }
	}
	pid = fork();
	if (pid == -1)  {
	    error("Can't fork; try again.\n");
	    exit(1);
	}
	if (pid) {
	    if (doencrypt) {
		static char msg[] = SECURE_MESSAGE;
		close(pv1[1]);
		close(pv2[0]);
#ifndef NOENCRYPTION
		des_enc_write(s, msg, sizeof(msg) - 1, schedule, &kdata->session);
#else
		write(s, msg, sizeof(msg) - 1);
#endif
	    } else {
		close(0);
		close(1);
	    }
	    close(2);
	    close(pv[1]);

	    if (s >= FD_SETSIZE || pv[0] >= FD_SETSIZE) {
		error ("fd too large\n");
		exit (1);
	    }

	    FD_ZERO(&readfrom);
	    FD_SET(s, &readfrom);
	    FD_SET(pv[0], &readfrom);
	    if (pv[0] > s)
		nfd = pv[0];
	    else
		nfd = s;
	    if (doencrypt) {
		if (pv2[1] >= FD_SETSIZE || pv1[0] >= FD_SETSIZE) {
		    error ("fd too large\n");
		    exit (1);
		}

		FD_ZERO(&writeto);
		FD_SET(pv2[1], &writeto);
		FD_SET(pv1[0], &readfrom);
		FD_SET(STDIN_FILENO, &readfrom);

		nfd = max(nfd, pv2[1]);
		nfd = max(nfd, pv1[0]);
	    } else
		ioctl(pv[0], FIONBIO, (char *)&one);

	    /* should set s nbio! */
	    nfd++;
	    do {
		ready = readfrom;
		if (doencrypt) {
		    wready = writeto;
		    if (select(nfd, &ready,
			       &wready, 0,
			       (struct timeval *) 0) < 0)
			break;
		} else
		    if (select(nfd, &ready, 0,
			       0, (struct timeval *)0) < 0)
			break;
		if (FD_ISSET(s, &ready)) {
		    int	ret;
		    if (doencrypt)
#ifndef NOENCRYPTION
			ret = des_enc_read(s, &sig, 1, schedule, &kdata->session);
#else
		    ret = read(s, &sig, 1);
#endif
		    else
			ret = read(s, &sig, 1);
		    if (ret <= 0)
			FD_CLR(s, &readfrom);
		    else
			kill(-pid, sig);
		}
		if (FD_ISSET(pv[0], &ready)) {
		    errno = 0;
		    cc = read(pv[0], buf, sizeof(buf));
		    if (cc <= 0) {
			shutdown(s, 1+1);
			FD_CLR(pv[0], &readfrom);
		    } else {
			if (doencrypt)
#ifndef NOENCRYPTION
			    des_enc_write(s, buf, cc, schedule, &kdata->session);
#else
			write(s, buf, cc);
#endif
			else
			    (void)
				write(s, buf, cc);
		    }
		}
		if (doencrypt && FD_ISSET(pv1[0], &ready)) {
		    errno = 0;
		    cc = read(pv1[0], buf, sizeof(buf));
		    if (cc <= 0) {
			shutdown(pv1[0], 1+1);
			FD_CLR(pv1[0], &readfrom);
		    } else
#ifndef NOENCRYPTION
			des_enc_write(STDOUT_FILENO, buf, cc, schedule, &kdata->session);
#else
		    write(STDOUT_FILENO, buf, cc);
#endif
		}

		if (doencrypt
		    && FD_ISSET(STDIN_FILENO, &ready)
		    && FD_ISSET(pv2[1], &wready)) {
		    errno = 0;
#ifndef NOENCRYPTION
		    cc = des_enc_read(STDIN_FILENO, buf, sizeof(buf), schedule, &kdata->session);
#else
		    cc = read(STDIN_FILENO, buf, sizeof(buf));
#endif
		    if (cc <= 0) {
			shutdown(STDIN_FILENO, 0);
			FD_CLR(STDIN_FILENO, &readfrom);
			close(pv2[1]);
			FD_CLR(pv2[1], &writeto);
		    } else
			write(pv2[1], buf, cc);
		}

	    } while (FD_ISSET(s, &readfrom) ||
		     (doencrypt && FD_ISSET(pv1[0], &readfrom)) ||
		     FD_ISSET(pv[0], &readfrom));
	    exit(0);
	}
	setsid();
	close(s);
	close(pv[0]);
	if (doencrypt) {
	    close(pv1[0]);
	    close(pv2[1]);
	    dup2(pv1[1], 1);
	    dup2(pv2[0], 0);
	    close(pv1[1]);
	    close(pv2[0]);
	}
	dup2(pv[1], 2);
	close(pv[1]);
    }
    if (*pwd->pw_shell == '\0')
	pwd->pw_shell = _PATH_BSHELL;
#ifdef HAVE_SETLOGIN
    if (setlogin(pwd->pw_name) < 0)
	syslog(LOG_ERR, "setlogin() failed: %m");
#endif

#ifdef HAVE_SETPCRED
    if (setpcred (pwd->pw_name, NULL) == -1)
	syslog(LOG_ERR, "setpcred() failure: %m");
#endif /* HAVE_SETPCRED */
    if(do_osfc2_magic(pwd->pw_uid))
	exit(1);
    setgid((gid_t)pwd->pw_gid);
    initgroups(pwd->pw_name, pwd->pw_gid);
    setuid((uid_t)pwd->pw_uid);
    strlcat(homedir, pwd->pw_dir, sizeof(homedir));

    /* Need to prepend path with BINDIR (/usr/athena/bin) to find rcp */
    snprintf(path, sizeof(path), "PATH=%s:%s", BINDIR, _PATH_DEFPATH);

    strlcat(shell, pwd->pw_shell, sizeof(shell));
    strlcpy(shell_path, pwd->pw_shell, sizeof(shell_path));
    strlcat(username, pwd->pw_name, sizeof(username));
    uid = pwd->pw_uid;
    cp = strrchr(pwd->pw_shell, '/');
    if (cp)
	cp++;
    else
	cp = pwd->pw_shell;
    endpwent();
    if (log_success || uid == 0) {
	if (use_kerberos)
	    syslog(LOG_INFO|LOG_AUTH,
		   "Kerberos shell from %s on %s as %s, cmd='%.80s'",
		   krb_unparse_name_long(kdata->pname, 
					 kdata->pinst, 
					 kdata->prealm),
		   remotehost, locuser, cmdbuf);
	else
	    syslog(LOG_INFO|LOG_AUTH, "%s@%s as %s: cmd='%.80s'",
		   remuser, remotehost, locuser, cmdbuf);
    }
    if (k_hasafs()) {
	char cell[64];

	if (new_pag)
	    k_setpag();	/* Put users process in an new pag */
	if (k_afs_cell_of_file (homedir, cell, sizeof(cell)) == 0)
	    krb_afslog_uid_home (cell, NULL, uid, homedir);
	krb_afslog_uid_home(NULL, NULL, uid, homedir);
    }
    execle(shell_path, cp, "-c", cmdbuf, 0, envinit);
    err(1, "%s", shell_path);
}

/*
 * Report error to client.  Note: can't be used until second socket has
 * connected to client, or older clients will hang waiting for that
 * connection first.
 */

static void
error(const char *fmt, ...)
{
    va_list ap;
    int len;
    char *bp, buf[BUFSIZ];

    va_start(ap, fmt);
    bp = buf;
    if (sent_null == 0) {
	*bp++ = 1;
	len = 1;
    } else
	len = 0;
    len += vsnprintf(bp, sizeof(buf) - len, fmt, ap);
    write(STDERR_FILENO, buf, len);
    va_end(ap);
}

static void
usage()
{

    syslog(LOG_ERR,
	   "usage: rshd [-alnkvxLPi] [-p port]");
    exit(2);
}
