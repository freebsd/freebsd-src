/*-
 * Copyright (c) 1983, 1988, 1989, 1993
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
 * remote login server:
 *	\0
 *	remuser\0
 *	locuser\0
 *	terminal_type/speed\0
 *	data
 */

#include "bsd_locl.h"

RCSID("$Id: rlogind.c,v 1.100 1997/05/25 01:15:20 assar Exp $");

extern int __check_rhosts_file;

char *INSECURE_MESSAGE =
"\r\n*** Connection not encrypted! Communication may be eavesdropped. ***"
"\r\n*** Use telnet or rlogin -x instead! ***\r\n";

#ifndef NOENCRYPTION
char *SECURE_MESSAGE =
"This rlogin session is using DES encryption for all transmissions.\r\n";
#else
#define	SECURE_MESSAGE INSECURE_MESSAGE
#endif

AUTH_DAT	*kdata;
KTEXT		ticket;
u_char		auth_buf[sizeof(AUTH_DAT)];
u_char		tick_buf[sizeof(KTEXT_ST)];
Key_schedule	schedule;
int		doencrypt, retval, use_kerberos, vacuous;

#define		ARGSTR			"Daip:lnkvxL:"

char	*env[2];
#define	NMAX 30
char	lusername[NMAX+1], rusername[NMAX+1];
static	char term[64] = "TERM=";
#define	ENVSIZE	(sizeof("TERM=")-1)	/* skip null for concatenation */
int	keepalive = 1;
int	check_all = 0;
int     no_delay = 0;

struct	passwd *pwd;

static const char *new_login = _PATH_LOGIN;

static void	doit (int, struct sockaddr_in *);
static int	control (int, char *, int);
static void	protocol (int, int);
static RETSIGTYPE cleanup (int);
void	fatal (int, const char *, int);
static int	do_rlogin (struct sockaddr_in *);
static void	setup_term (int);
static int	do_krb_login (struct sockaddr_in *);
static void	usage (void);

static int
readstream(int p, char *ibuf, int bufsize)
{
#ifndef HAVE_GETMSG
    return read(p, ibuf, bufsize);
#else
    static int flowison = -1;  /* current state of flow: -1 is unknown */
    static struct strbuf strbufc, strbufd;
    static unsigned char ctlbuf[BUFSIZ];
    static int use_read = 1;

    int flags = 0;
    int ret;
    struct termios tsp;

    struct iocblk ip;
    char vstop, vstart;
    int ixon;
    int newflow;

    if (use_read)
	{
	    ret = read(p, ibuf, bufsize);
	    if (ret < 0 && errno == EBADMSG)
		use_read = 0;
	    else
		return ret;
	}

    strbufc.maxlen = BUFSIZ;
    strbufc.buf = (char *)ctlbuf;
    strbufd.maxlen = bufsize-1;
    strbufd.len = 0;
    strbufd.buf = ibuf+1;
    ibuf[0] = 0;

    ret = getmsg(p, &strbufc, &strbufd, &flags);
    if (ret < 0)  /* error of some sort -- probably EAGAIN */
	return(-1);

    if (strbufc.len <= 0 || ctlbuf[0] == M_DATA) {
	/* data message */
	if (strbufd.len > 0) {			/* real data */
	    return(strbufd.len + 1);	/* count header char */
	} else {
	    /* nothing there */
	    errno = EAGAIN;
	    return(-1);
	}
    }

    /*
     * It's a control message.  Return 1, to look at the flag we set
     */

    switch (ctlbuf[0]) {
    case M_FLUSH:
	if (ibuf[1] & FLUSHW)
	    ibuf[0] = TIOCPKT_FLUSHWRITE;
	return(1);

    case M_IOCTL:
	memcpy(&ip, (ibuf+1), sizeof(ip));

	switch (ip.ioc_cmd) {
#ifdef TCSETS
	case TCSETS:
	case TCSETSW:
	case TCSETSF:
	    memcpy(&tsp,
		   (ibuf+1 + sizeof(struct iocblk)),
		   sizeof(tsp));
	    vstop = tsp.c_cc[VSTOP];
	    vstart = tsp.c_cc[VSTART];
	    ixon = tsp.c_iflag & IXON;
	    break;
#endif
	default:
	    errno = EAGAIN;
	    return(-1);
	}

	newflow =  (ixon && (vstart == 021) && (vstop == 023)) ? 1 : 0;
	if (newflow != flowison) {  /* it's a change */
	    flowison = newflow;
	    ibuf[0] = newflow ? TIOCPKT_DOSTOP : TIOCPKT_NOSTOP;
	    return(1);
	}
    }

    /* nothing worth doing anything about */
    errno = EAGAIN;
    return(-1);
#endif
}

#ifdef HAVE_UTMPX_H
static int
logout(const char *line)
{
    struct utmpx utmpx, *utxp;
    int ret = 1;

    setutxent ();
    memset(&utmpx, 0, sizeof(utmpx));
    utmpx.ut_type = USER_PROCESS;
    strncpy(utmpx.ut_line, line, sizeof(utmpx.ut_line));
    utxp = getutxline(&utmpx);
    if (utxp) {
	strcpy(utxp->ut_user, "");
	utxp->ut_type = DEAD_PROCESS;
#ifdef _STRUCT___EXIT_STATUS
	utxp->ut_exit.__e_termination = 0;
	utxp->ut_exit.__e_exit = 0;
#elif defined(__osf__) /* XXX */
	utxp->ut_exit.ut_termination = 0;
	utxp->ut_exit.ut_exit = 0;
#else	
	utxp->ut_exit.e_termination = 0;
	utxp->ut_exit.e_exit = 0;
#endif
	gettimeofday(&utxp->ut_tv, NULL);
	pututxline(utxp);
#ifdef WTMPX_FILE
	updwtmpx(WTMPX_FILE, utxp);
#else
	ret = 0;
#endif
    }
    endutxent();
    return ret;
}
#else
static int
logout(const char *line)
{
    FILE *fp;
    struct utmp ut;
    int rval;

    if (!(fp = fopen(_PATH_UTMP, "r+")))
	return(0);
    rval = 1;
    while (fread(&ut, sizeof(struct utmp), 1, fp) == 1) {
	if (!ut.ut_name[0] ||
	    strncmp(ut.ut_line, line, sizeof(ut.ut_line)))
	    continue;
	memset(ut.ut_name, 0, sizeof(ut.ut_name));
#ifdef HAVE_UT_HOST
	memset(ut.ut_host, 0, sizeof(ut.ut_host));
#endif
	time(&ut.ut_time);
	fseek(fp, (long)-sizeof(struct utmp), SEEK_CUR);
	fwrite(&ut, sizeof(struct utmp), 1, fp);
	fseek(fp, (long)0, SEEK_CUR);
	rval = 0;
    }
    fclose(fp);
    return(rval);
}
#endif

#ifndef HAVE_LOGWTMP
static void
logwtmp(const char *line, const char *name, const char *host)
{
    struct utmp ut;
    struct stat buf;
    int fd;

    memset (&ut, 0, sizeof(ut));
    if ((fd = open(_PATH_WTMP, O_WRONLY|O_APPEND, 0)) < 0)
	return;
    if (!fstat(fd, &buf)) {
	strncpy(ut.ut_line, line, sizeof(ut.ut_line));
	strncpy(ut.ut_name, name, sizeof(ut.ut_name));
#ifdef HAVE_UT_HOST
	strncpy(ut.ut_host, host, sizeof(ut.ut_host));
#endif
#ifdef HAVE_UT_PID
	ut.ut_pid = getpid();
#endif
#ifdef HAVE_UT_TYPE
	if(name[0])
	    ut.ut_type = USER_PROCESS;
	else
	    ut.ut_type = DEAD_PROCESS;
#endif
	time(&ut.ut_time);
	if (write(fd, &ut, sizeof(struct utmp)) !=
	    sizeof(struct utmp))
	    ftruncate(fd, buf.st_size);
    }
    close(fd);
}
#endif

int
main(int argc, char **argv)
{
    struct sockaddr_in from;
    int ch, fromlen, on;
    int interactive = 0;
    int portnum = 0;

    set_progname(argv[0]);

    openlog("rlogind", LOG_PID | LOG_CONS, LOG_AUTH);

    opterr = 0;
    while ((ch = getopt(argc, argv, ARGSTR)) != EOF)
	switch (ch) {
	case 'D':
	    no_delay = 1;
	    break;
	case 'a':
	    break;
	case 'i':
	    interactive = 1;
	    break;
	case 'p':
	    portnum = htons(atoi(optarg));
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
	    new_login = optarg;
	    break;
	case '?':
	default:
	    usage();
	    break;
	}
    argc -= optind;
    argv += optind;

    if (use_kerberos && vacuous) {
	usage();
	fatal(STDERR_FILENO, "only one of -k and -v allowed", 0);
    }
    if (interactive) {
	if(portnum == 0)
	    portnum = get_login_port (use_kerberos, doencrypt);
	mini_inetd (portnum);
    }

    fromlen = sizeof (from);
    if (getpeername(0, (struct sockaddr *)&from, &fromlen) < 0) {
	syslog(LOG_ERR,"Can't get peer name of remote host: %m");
	fatal(STDERR_FILENO, "Can't get peer name of remote host", 1);
    }
    on = 1;
#ifdef HAVE_SETSOCKOPT
#ifdef SO_KEEPALIVE
    if (keepalive &&
	setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, (void *)&on,
		   sizeof (on)) < 0)
	syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
#endif
#ifdef TCP_NODELAY
    if (no_delay &&
	setsockopt(0, IPPROTO_TCP, TCP_NODELAY, (void *)&on,
		   sizeof(on)) < 0)
	syslog(LOG_WARNING, "setsockopt (TCP_NODELAY): %m");
#endif

#ifdef IP_TOS
    on = IPTOS_LOWDELAY;
    if (setsockopt(0, IPPROTO_IP, IP_TOS, (void *)&on, sizeof(int)) < 0)
	syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
#endif
#endif /* HAVE_SETSOCKOPT */
    doit(0, &from);
    return 0;
}

int	child;
int	netf;
char	line[MaxPathLen];
int	confirmed;

struct winsize win = { 0, 0, 0, 0 };


static void
doit(int f, struct sockaddr_in *fromp)
{
    int master, pid, on = 1;
    int authenticated = 0;
    char hostname[2 * MaxHostNameLen + 1];
    char c;

    alarm(60);
    read(f, &c, 1);

    if (c != 0)
	exit(1);
    if (vacuous)
	fatal(f, "Remote host requires Kerberos authentication", 0);

    alarm(0);
    inaddr2str (fromp->sin_addr, hostname, sizeof(hostname));

    if (use_kerberos) {
	retval = do_krb_login(fromp);
	if (retval == 0)
	    authenticated++;
	else if (retval > 0)
	    fatal(f, krb_get_err_text(retval), 0);
	write(f, &c, 1);
	confirmed = 1;		/* we sent the null! */
    } else {
	fromp->sin_port = ntohs((u_short)fromp->sin_port);
	if (fromp->sin_family != AF_INET ||
	    fromp->sin_port >= IPPORT_RESERVED ||
	    fromp->sin_port < IPPORT_RESERVED/2) {
	    syslog(LOG_NOTICE, "Connection from %s on illegal port",
		   inet_ntoa(fromp->sin_addr));
	    fatal(f, "Permission denied", 0);
	}
	ip_options_and_die (0, fromp);
	if (do_rlogin(fromp) == 0)
	    authenticated++;
    }
    if (confirmed == 0) {
	write(f, "", 1);
	confirmed = 1;		/* we sent the null! */
    }
#ifndef NOENCRYPTION
    if (doencrypt)
	des_enc_write(f, SECURE_MESSAGE,
		      strlen(SECURE_MESSAGE),
		      schedule, &kdata->session);
    else
#endif
	write(f, INSECURE_MESSAGE, strlen(INSECURE_MESSAGE));
    netf = f;

    pid = forkpty(&master, line, NULL, NULL);
    if (pid < 0) {
	if (errno == ENOENT)
	    fatal(f, "Out of ptys", 0);
	else
	    fatal(f, "Forkpty", 1);
    }
    if (pid == 0) {
	if (f > 2)	/* f should always be 0, but... */
	    close(f);
	setup_term(0);
	if (lusername[0] == '-'){
	    syslog(LOG_ERR, "tried to pass user \"%s\" to login",
		   lusername);
	    fatal(STDERR_FILENO, "invalid user", 0);
	}
	if (authenticated) {
	    if (use_kerberos && (pwd->pw_uid == 0))
		syslog(LOG_INFO|LOG_AUTH,
		       "ROOT Kerberos login from %s on %s\n",
		       krb_unparse_name_long(kdata->pname, 
					     kdata->pinst, 
					     kdata->prealm), 
		       hostname);
		    
	    execl(new_login, "login", "-p",
		  "-h", hostname, "-f", "--", lusername, 0);
	} else
	    execl(new_login, "login", "-p",
		  "-h", hostname, "--", lusername, 0);
	fatal(STDERR_FILENO, new_login, 1);
	/*NOTREACHED*/
    }
    /*
     * If encrypted, don't turn on NBIO or the des read/write
     * routines will croak.
     */

    if (!doencrypt)
	ioctl(f, FIONBIO, &on);
    ioctl(master, FIONBIO, &on);
    ioctl(master, TIOCPKT, &on);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGCHLD, cleanup);
    setsid();
    protocol(f, master);
    signal(SIGCHLD, SIG_IGN);
    cleanup(0);
}

const char	magic[2] = { 0377, 0377 };

/*
 * Handle a "control" request (signaled by magic being present)
 * in the data stream.  For now, we are only willing to handle
 * window size changes.
 */
static int
control(int master, char *cp, int n)
{
    struct winsize w;
    char *p;
    u_int32_t tmp;

    if (n < 4 + 4 * sizeof (u_int16_t) || cp[2] != 's' || cp[3] != 's')
	return (0);
#ifdef TIOCSWINSZ
    p = cp + 4;
    p += krb_get_int(p, &tmp, 2, 0);
    w.ws_row = tmp;
    p += krb_get_int(p, &tmp, 2, 0);
    w.ws_col = tmp;

    p += krb_get_int(p, &tmp, 2, 0);
#ifdef HAVE_WS_XPIXEL
    w.ws_xpixel = tmp;
#endif
    p += krb_get_int(p, &tmp, 2, 0);
#ifdef HAVE_WS_YPIXEL
    w.ws_ypixel = tmp;
#endif
    ioctl(master, TIOCSWINSZ, &w);
#endif
    return p - cp;
}

static
void
send_oob(int fd, char c)
{
    static char last_oob = 0xFF;

#if (SunOS == 5) || defined(__hpux)
    /*
     * PSoriasis and HP-UX always send TIOCPKT_DOSTOP at startup so we
     * can avoid sending OOB data and thus not break on Linux by merging
     * TIOCPKT_DOSTOP into the first TIOCPKT_WINDOW.
     */
    static int oob_kludge = 2;
    if (oob_kludge == 2)
	{
	    oob_kludge--;		/* First time send nothing */
	    return;
	}
    else if (oob_kludge == 1)
	{
	    oob_kludge--;		/* Second time merge TIOCPKT_WINDOW */
	    c |= TIOCPKT_WINDOW;
	}
#endif

#define	pkcontrol(c) ((c)&(TIOCPKT_FLUSHWRITE|TIOCPKT_NOSTOP|TIOCPKT_DOSTOP))
    c = pkcontrol(c);
    /* Multiple OOB data breaks on Linux, avoid it when possible. */
    if (c != last_oob)
	send(fd, &c, 1, MSG_OOB);
    last_oob = c;
}

/*
 * rlogin "protocol" machine.
 */
static void
protocol(int f, int master)
{
    char pibuf[1024+1], fibuf[1024], *pbp, *fbp;
    int pcc = 0, fcc = 0;
    int cc, nfd, n;
    char cntl;
    unsigned char oob_queue = 0;

    /*
     * Must ignore SIGTTOU, otherwise we'll stop
     * when we try and set slave pty's window shape
     * (our controlling tty is the master pty).
     */
    signal(SIGTTOU, SIG_IGN);

    send_oob(f, TIOCPKT_WINDOW); /* indicate new rlogin */

    if (f > master)
	nfd = f + 1;
    else
	nfd = master + 1;
    if (nfd > FD_SETSIZE) {
	syslog(LOG_ERR, "select mask too small, increase FD_SETSIZE");
	fatal(f, "internal error (select mask too small)", 0);
    }
    for (;;) {
	fd_set ibits, obits, ebits, *omask;

	FD_ZERO(&ebits);
	FD_ZERO(&ibits);
	FD_ZERO(&obits);
	omask = (fd_set *)NULL;
	if (fcc) {
	    FD_SET(master, &obits);
	    omask = &obits;
	} else
	    FD_SET(f, &ibits);
	if (pcc >= 0)
	    if (pcc) {
		FD_SET(f, &obits);
		omask = &obits;
	    } else
		FD_SET(master, &ibits);
	FD_SET(master, &ebits);
	if ((n = select(nfd, &ibits, omask, &ebits, 0)) < 0) {
	    if (errno == EINTR)
		continue;
	    fatal(f, "select", 1);
	}
	if (n == 0) {
	    /* shouldn't happen... */
	    sleep(5);
	    continue;
	}
	if (FD_ISSET(master, &ebits)) {
	    cc = readstream(master, &cntl, 1);
	    if (cc == 1 && pkcontrol(cntl)) {
#if 0				/* Kludge around */
		send_oob(f, cntl);
#endif
		oob_queue = cntl;
		if (cntl & TIOCPKT_FLUSHWRITE) {
		    pcc = 0;
		    FD_CLR(master, &ibits);
		}
	    }
	}
	if (FD_ISSET(f, &ibits)) {
#ifndef NOENCRYPTION
	    if (doencrypt)
		fcc = des_enc_read(f, fibuf,
				   sizeof(fibuf),
				   schedule, &kdata->session);
	    else
#endif
		fcc = read(f, fibuf, sizeof(fibuf));
	    if (fcc < 0 && errno == EWOULDBLOCK)
		fcc = 0;
	    else {
		char *cp;
		int left, n;

		if (fcc <= 0)
		    break;
		fbp = fibuf;

	    top:
		for (cp = fibuf; cp < fibuf+fcc-1; cp++)
		    if (cp[0] == magic[0] &&
			cp[1] == magic[1]) {
			left = fcc - (cp-fibuf);
			n = control(master, cp, left);
			if (n) {
			    left -= n;
			    if (left > 0)
				memmove(cp, cp+n, left);
			    fcc -= n;
			    goto top; /* n^2 */
			}
		    }
		FD_SET(master, &obits);		/* try write */
	    }
	}

	if (FD_ISSET(master, &obits) && fcc > 0) {
	    cc = write(master, fbp, fcc);
	    if (cc > 0) {
		fcc -= cc;
		fbp += cc;
	    }
	}

	if (FD_ISSET(master, &ibits)) {
	    pcc = readstream(master, pibuf, sizeof (pibuf));
	    pbp = pibuf;
	    if (pcc < 0 && errno == EWOULDBLOCK)
		pcc = 0;
	    else if (pcc <= 0)
		break;
	    else if (pibuf[0] == 0) {
		pbp++, pcc--;
		if (!doencrypt)
		    FD_SET(f, &obits);	/* try write */
	    } else {
		if (pkcontrol(pibuf[0])) {
		    oob_queue = pibuf[0];
#if 0				/* Kludge around */
		    send_oob(f, pibuf[0]);
#endif
		}
		pcc = 0;
	    }
	}
	if ((FD_ISSET(f, &obits)) && pcc > 0) {
#ifndef NOENCRYPTION
	    if (doencrypt)
		cc = des_enc_write(f, pbp, pcc, schedule, &kdata->session);
	    else
#endif
		cc = write(f, pbp, pcc);
	    if (cc < 0 && errno == EWOULDBLOCK) {
		/*
		 * This happens when we try write after read
		 * from p, but some old kernels balk at large
		 * writes even when select returns true.
		 */
		if (!FD_ISSET(master, &ibits))
		    sleep(5);
		continue;
	    }
	    if (cc > 0) {
		pcc -= cc;
		pbp += cc;
		/* Only send urg data when normal data
		 * has just been sent.
		 * Linux has deep problems with more
		 * than one byte of OOB data.
		 */
		if (oob_queue) {
		    send_oob (f, oob_queue);
		    oob_queue = 0;
		}
	    }
	}
    }
}

static RETSIGTYPE
cleanup(int signo)
{
    char *p = clean_ttyname (line);

    if (logout(p) == 0)
	logwtmp(p, "", "");
    chmod(line, 0666);
    chown(line, 0, 0);
    *p = 'p';
    chmod(line, 0666);
    chown(line, 0, 0);
    shutdown(netf, 2);
    signal(SIGHUP, SIG_IGN);
#ifdef HAVE_VHANGUP
    vhangup();
#endif /* HAVE_VHANGUP */
    exit(1);
}

void
fatal(int f, const char *msg, int syserr)
{
    int len;
    char buf[BUFSIZ], *bp = buf;

    /*
     * Prepend binary one to message if we haven't sent
     * the magic null as confirmation.
     */
    if (!confirmed)
	*bp++ = '\01';		/* error indicator */
    if (syserr)
	snprintf(bp, sizeof(buf) - (bp - buf),
		 "rlogind: %s: %s.\r\n",
		 msg, strerror(errno));
    else
	snprintf(bp, sizeof(buf) - (bp - buf),
		 "rlogind: %s.\r\n", msg);
    len = strlen(bp);
#ifndef NOENCRYPTION
    if (doencrypt)
	des_enc_write(f, buf, bp + len - buf, schedule, &kdata->session);
    else
#endif
	write(f, buf, bp + len - buf);
    exit(1);
}

static void
xgetstr(char *buf, int cnt, char *errmsg)
{
    char c;

    do {
	if (read(0, &c, 1) != 1)
	    exit(1);
	if (--cnt < 0)
	    fatal(STDOUT_FILENO, errmsg, 0);
	*buf++ = c;
    } while (c != 0);
}

static int
do_rlogin(struct sockaddr_in *dest)
{
    xgetstr(rusername, sizeof(rusername), "remuser too long");
    xgetstr(lusername, sizeof(lusername), "locuser too long");
    xgetstr(term+ENVSIZE, sizeof(term)-ENVSIZE, "Terminal type too long");

    pwd = k_getpwnam(lusername);
    if (pwd == NULL)
	return (-1);
    if (pwd->pw_uid == 0 && strcmp("root", lusername) != 0)
	{
	    syslog(LOG_ALERT, "NIS attack, user %s has uid 0", lusername);
	    return (-1);
	}
    return (iruserok(dest->sin_addr.s_addr,
		     (pwd->pw_uid == 0),
		     rusername,
		     lusername));
}

static void 
setup_term(int fd)
{
    char *cp = strchr(term+ENVSIZE, '/');
    char *speed;
    struct termios tt;

    tcgetattr(fd, &tt);
    if (cp) {
	int s;

	*cp++ = '\0';
	speed = cp;
	cp = strchr(speed, '/');
	if (cp)
	    *cp++ = '\0';
	s = int2speed_t (atoi (speed));
	if (s > 0) {
	    cfsetospeed (&tt, s);
	    cfsetispeed (&tt, s);
	}
    }

    tt.c_iflag &= ~INPCK;
    tt.c_iflag |= ICRNL|IXON;
    tt.c_oflag |= OPOST|ONLCR;
#ifdef TAB3
    tt.c_oflag |= TAB3;
#endif /* TAB3 */
#ifdef ONLRET
    tt.c_oflag &= ~ONLRET;
#endif /* ONLRET */
    tt.c_lflag |= (ECHO|ECHOE|ECHOK|ISIG|ICANON);
    tt.c_cflag &= ~PARENB;
    tt.c_cflag |= CS8;
    tt.c_cc[VMIN] = 1;
    tt.c_cc[VTIME] = 0;
    tt.c_cc[VEOF] = CEOF;
    tcsetattr(fd, TCSAFLUSH, &tt);

    env[0] = term;
    env[1] = 0;
    environ = env;
}

#define	VERSION_SIZE	9

/*
 * Do the remote kerberos login to the named host with the
 * given inet address
 *
 * Return 0 on valid authorization
 * Return -1 on valid authentication, no authorization
 * Return >0 for error conditions
 */
static int
do_krb_login(struct sockaddr_in *dest)
{
    int rc;
    char instance[INST_SZ], version[VERSION_SIZE];
    long authopts = 0L;	/* !mutual */
    struct sockaddr_in faddr;

    kdata = (AUTH_DAT *) auth_buf;
    ticket = (KTEXT) tick_buf;

    k_getsockinst(0, instance, sizeof(instance));

    if (doencrypt) {
	rc = sizeof(faddr);
	if (getsockname(0, (struct sockaddr *)&faddr, &rc))
	    return (-1);
	authopts = KOPT_DO_MUTUAL;
	rc = krb_recvauth(
			  authopts, 0,
			  ticket, "rcmd",
			  instance, dest, &faddr,
			  kdata, "", schedule, version);
	des_set_key(&kdata->session, schedule);

    } else
	rc = krb_recvauth(
			  authopts, 0,
			  ticket, "rcmd",
			  instance, dest, (struct sockaddr_in *) 0,
			  kdata, "", 0, version);

    if (rc != KSUCCESS)
	return (rc);

    xgetstr(lusername, sizeof(lusername), "locuser");
    /* get the "cmd" in the rcmd protocol */
    xgetstr(term+ENVSIZE, sizeof(term)-ENVSIZE, "Terminal type");

    pwd = k_getpwnam(lusername);
    if (pwd == NULL)
	return (-1);
    if (pwd->pw_uid == 0 && strcmp("root", lusername) != 0)
	{
	    syslog(LOG_ALERT, "NIS attack, user %s has uid 0", lusername);
	    return (-1);
	}

    /* returns nonzero for no access */
    if (kuserok(kdata, lusername) != 0)
	return (-1);

    return (0);

}

static void
usage(void)
{
    syslog(LOG_ERR,
	   "usage: rlogind [-Dailn] [-p port] [-x] [-L login] [-k | -v]");
    exit(1);
}
