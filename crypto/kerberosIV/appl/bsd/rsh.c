/*-
 * Copyright (c) 1983, 1990 The Regents of the University of California.
 * All rights reserved.
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

#include "bsd_locl.h"

RCSID("$Id: rsh.c,v 1.43.2.2 2000/10/10 12:53:50 assar Exp $");

CREDENTIALS cred;
Key_schedule schedule;
int use_kerberos = 1, doencrypt;
char dst_realm_buf[REALM_SZ], *dest_realm;

/*
 * rsh - remote shell
 */
int rfd2;

static void
usage(void)
{
    fprintf(stderr,
	    "usage: rsh [-ndKx] [-k realm] [-p port] [-l login] host [command]\n");
    exit(1);
}

static char *
copyargs(char **argv)
{
    int cc;
    char **ap, *p;
    char *args;

    cc = 0;
    for (ap = argv; *ap; ++ap)
	cc += strlen(*ap) + 1;
    args = malloc(cc);
    if (args == NULL)
	errx(1, "Out of memory.");
    for (p = args, ap = argv; *ap; ++ap) {
	strcpy(p, *ap);
	while(*p)
	    ++p;
	if (ap[1])
	    *p++ = ' ';
    }
    return(args);
}

static RETSIGTYPE
sendsig(int signo_)
{
    char signo = signo_;
#ifndef NOENCRYPTION
    if (doencrypt)
	des_enc_write(rfd2, &signo, 1, schedule, &cred.session);
    else
#endif
	write(rfd2, &signo, 1);
}

static void
talk(int nflag, sigset_t omask, int pid, int rem)
{
    int cc, wc;
    char *bp;
    fd_set readfrom, ready, rembits;
    char buf[DES_RW_MAXWRITE];

    if (pid == 0) {
	if (nflag)
	    goto done;

	close(rfd2);

    reread:		errno = 0;
    if ((cc = read(0, buf, sizeof buf)) <= 0)
	goto done;
    bp = buf;

    rewrite:   
    FD_ZERO(&rembits);
    if (rem >= FD_SETSIZE)
	errx(1, "fd too large");
    FD_SET(rem, &rembits);
    if (select(rem + 1, 0, &rembits, 0, 0) < 0) {
	if (errno != EINTR) 
	    err(1, "select");
	goto rewrite;
    }
    if (!FD_ISSET(rem, &rembits))
	goto rewrite;
#ifndef NOENCRYPTION
    if (doencrypt)
	wc = des_enc_write(rem, bp, cc, schedule, &cred.session);
    else
#endif
	wc = write(rem, bp, cc);
    if (wc < 0) {
	if (errno == EWOULDBLOCK)
	    goto rewrite;
	goto done;
    }
    bp += wc;
    cc -= wc;
    if (cc == 0)
	goto reread;
    goto rewrite;
    done:
    shutdown(rem, 1);
    exit(0);
    }

    if (sigprocmask(SIG_SETMASK, &omask, 0) != 0)
	warn("sigprocmask");
    FD_ZERO(&readfrom);
    if (rem >= FD_SETSIZE || rfd2 >= FD_SETSIZE)
	errx(1, "fd too large");
    FD_SET(rem, &readfrom);
    FD_SET(rfd2, &readfrom);
    do {
	ready = readfrom;
	if (select(max(rem,rfd2)+1, &ready, 0, 0, 0) < 0) {
	    if (errno != EINTR)
		err(1, "select");
	    continue;
	}
	if (FD_ISSET(rfd2, &ready)) {
	    errno = 0;
#ifndef NOENCRYPTION
	    if (doencrypt)
		cc = des_enc_read(rfd2, buf, sizeof buf,
				  schedule, &cred.session);
	    else
#endif
		cc = read(rfd2, buf, sizeof buf);
	    if (cc <= 0) {
		if (errno != EWOULDBLOCK)
		    FD_CLR(rfd2, &readfrom);
	    } else
		write(2, buf, cc);
	}
	if (FD_ISSET(rem, &ready)) {
	    errno = 0;
#ifndef NOENCRYPTION
	    if (doencrypt)
		cc = des_enc_read(rem, buf, sizeof buf,
				  schedule, &cred.session);
	    else
#endif
		cc = read(rem, buf, sizeof buf);
	    if (cc <= 0) {
		if (errno != EWOULDBLOCK)
		    FD_CLR(rem, &readfrom);
	    } else
		write(1, buf, cc);
	}
    } while (FD_ISSET(rfd2, &readfrom) || FD_ISSET(rem, &readfrom));
}

int
main(int argc, char **argv)
{
    struct passwd *pw;
    int sv_port, user_port = 0;
    sigset_t omask;
    int argoff, ch, dflag, nflag, nfork, one, pid, rem, uid;
    char *args, *host, *user, *local_user;

    argoff = dflag = nflag = nfork = 0;
    one = 1;
    host = user = NULL;
    pid = 1;

    set_progname(argv[0]);

    /* handle "rsh host flags" */
    if (argc > 2 && argv[1][0] != '-') {
	host = argv[1];
	argoff = 1;
    }

#define	OPTIONS	"+8KLde:k:l:np:wx"
    while ((ch = getopt(argc - argoff, argv + argoff, OPTIONS)) != -1)
	switch(ch) {
	case 'K':
	    use_kerberos = 0;
	    break;
	case 'L':	/* -8Lew are ignored to allow rlogin aliases */
	case 'e':
	case 'w':
	case '8':
	    break;
	case 'd':
	    dflag = 1;
	    break;
	case 'l':
	    user = optarg;
	    break;
	case 'k':
	    dest_realm = dst_realm_buf;
	    strlcpy(dest_realm, optarg, REALM_SZ);
	    break;
	case 'n':
	    nflag = nfork = 1;
	    break;
	case 'x':
	    doencrypt = 1;
	    break;
	case 'p': {
	    char *endptr;

	    user_port = strtol (optarg, &endptr, 0);
	    if (user_port == 0 && optarg == endptr)
		errx (1, "Bad port `%s'", optarg);
	    user_port = htons(user_port);
	    break;
	}
	case '?':
	default:
	    usage();
	}
    optind += argoff;

    /* if haven't gotten a host yet, do so */
    if (!host && !(host = argv[optind++]))
	usage();

    /* if no further arguments, must have been called as rlogin. */
    if (!argv[optind]) {
	*argv = "rlogin";
	paranoid_setuid (getuid ());
	execv(_PATH_RLOGIN, argv);
	err(1, "can't exec %s", _PATH_RLOGIN);
    }

#ifndef __CYGWIN32__
    if (!(pw = k_getpwuid(uid = getuid())))
	errx(1, "unknown user id.");
    local_user = pw->pw_name;
    if (!user)
	user = local_user;
#else
    if (!user)
	errx(1, "Sorry, you need to specify the username (with -l)");
    local_user = user;
#endif

    /* -n must still fork but does not turn of the -n functionality */
    if (doencrypt)
	nfork = 0;

    args = copyargs(argv+optind);

    if (user_port)
	sv_port = user_port;
    else
	sv_port = get_shell_port(use_kerberos, doencrypt);

    if (use_kerberos) {
	paranoid_setuid(getuid());
	rem = KSUCCESS;
	errno = 0;
	if (dest_realm == NULL)
	    dest_realm = krb_realmofhost(host);

	if (doencrypt)
	    rem = krcmd_mutual(&host, sv_port, user, args,
			       &rfd2, dest_realm, &cred, schedule);
	else
	    rem = krcmd(&host, sv_port, user, args, &rfd2,
			dest_realm);
	if (rem < 0) {
	    int i = 0;
	    char **newargv;

	    if (errno == ECONNREFUSED)
		warning("remote host doesn't support Kerberos");
	    if (errno == ENOENT)
		warning("can't provide Kerberos auth data");
	    newargv = malloc((argc + 2) * sizeof(*newargv));
	    if (newargv == NULL)
		err(1, "malloc");
	    newargv[i] = argv[i];
	    ++i;
	    if (argv[i][0] != '-') {
		newargv[i] = argv[i];
		++i;
	    }
	    newargv[i++] = "-K";
	    for(; i <= argc; ++i)
		newargv[i] = argv[i - 1];
	    newargv[argc + 1] = NULL;
	    execv(_PATH_RSH, newargv);
	}
    } else {
	if (doencrypt)
	    errx(1, "the -x flag requires Kerberos authentication.");
	if (geteuid() != 0)
	    errx(1, "not installed setuid root, "
		 "only root may use non kerberized rsh");
	rem = rcmd(&host, sv_port, local_user, user, args, &rfd2);
    }

    if (rem < 0)
	exit(1);

    if (rfd2 < 0)
	errx(1, "can't establish stderr.");
#if defined(SO_DEBUG) && defined(HAVE_SETSOCKOPT)
    if (dflag) {
	if (setsockopt(rem, SOL_SOCKET, SO_DEBUG, (void *)&one,
		       sizeof(one)) < 0)
	    warn("setsockopt");
	if (setsockopt(rfd2, SOL_SOCKET, SO_DEBUG, (void *)&one,
		       sizeof(one)) < 0)
	    warn("setsockopt");
    }
#endif

    paranoid_setuid(uid);
    {
	sigset_t sigmsk;
	sigemptyset(&sigmsk);
	sigaddset(&sigmsk, SIGINT);
	sigaddset(&sigmsk, SIGQUIT);
	sigaddset(&sigmsk, SIGTERM);
	if (sigprocmask(SIG_BLOCK, &sigmsk, &omask) != 0)
	    warn("sigprocmask");
    }
    if (signal(SIGINT, SIG_IGN) != SIG_IGN)
	signal(SIGINT, sendsig);
    if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
	signal(SIGQUIT, sendsig);
    if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
	signal(SIGTERM, sendsig);
    signal(SIGPIPE, SIG_IGN);

    if (!nfork) {
	pid = fork();
	if (pid < 0)
	    err(1, "fork");
    }

    if (!doencrypt) {
	ioctl(rfd2, FIONBIO, &one);
	ioctl(rem, FIONBIO, &one);
    }

    talk(nflag, omask, pid, rem);
    
    if (!nflag)
	kill(pid, SIGKILL);
    exit(0);
}
