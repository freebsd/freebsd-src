/*-
 * Copyright (c) 1980, 1991, 1993, 1994
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)w.c	8.4 (Berkeley) 4/16/94";
#endif
static const char rcsid[] =
	"$Id: w.c,v 1.21 1997/08/25 06:42:19 charnier Exp $";
#endif /* not lint */

/*
 * w - print system status (who and what)
 *
 * This program is similar to the systat command on Tenex/Tops 10/20
 *
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/tty.h>

#include <machine/cpu.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include <vis.h>
#include <locale.h>

#include <arpa/nameser.h>
#include <resolv.h>

#include "extern.h"

struct timeval	boottime;
struct utmp	utmp;
struct winsize	ws;
kvm_t	       *kd;
time_t		now;		/* the current time of day */
time_t		uptime;		/* time of last reboot & elapsed time since */
int		ttywidth;	/* width of tty */
int		argwidth;	/* width of tty */
int		header = 1;	/* true if -h flag: don't print heading */
int		nflag;		/* true if -n flag: don't convert addrs */
int		dflag;		/* true if -d flag: output debug info */
int		sortidle;	/* sort bu idle time */
char	       *sel_user;	/* login of particular user selected */
char		domain[MAXHOSTNAMELEN];

/*
 * One of these per active utmp entry.
 */
struct	entry {
	struct	entry *next;
	struct	utmp utmp;
	dev_t	tdev;			/* dev_t of terminal */
	time_t	idle;			/* idle time of terminal in seconds */
	struct	kinfo_proc *kp;		/* `most interesting' proc */
	char	*args;			/* arg list of interesting process */
	struct	kinfo_proc *dkp;	/* debug option proc list */
} *ep, *ehead = NULL, **nextp = &ehead;

static void	 pr_header __P((time_t *, int));
static struct stat
		*ttystat __P((char *));
static void	 usage __P((int));

char *fmt_argv __P((char **, char *, int));	/* ../../bin/ps/fmt.c */

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern char *__progname;
	struct kinfo_proc *kp;
	struct kinfo_proc *dkp;
	struct hostent *hp;
	struct stat *stp;
	FILE *ut;
	u_long l;
	size_t arglen;
	int ch, i, nentries, nusers, wcmd, longidle;
	char *memf, *nlistf, *p, *vis_args, *x;
	char buf[MAXHOSTNAMELEN], errbuf[256];

	(void) setlocale(LC_ALL, "");

	/* Are we w(1) or uptime(1)? */
	p = __progname;
	if (*p == '-')
		p++;
	if (*p == 'u') {
		wcmd = 0;
		p = "";
	} else {
		wcmd = 1;
		p = "dhiflM:N:nsuw";
	}

	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, p)) != -1)
		switch (ch) {
		case 'd':
			dflag = 1;
			break;
		case 'h':
			header = 0;
			break;
		case 'i':
			sortidle = 1;
			break;
		case 'M':
			header = 0;
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'f': case 'l': case 's': case 'u': case 'w':
			warnx("[-flsuw] no longer supported");
			/* FALLTHROUGH */
		case '?':
		default:
			usage(wcmd);
		}
	argc -= optind;
	argv += optind;

	if (!(_res.options & RES_INIT))
		res_init();
	_res.retrans = 2;	/* resolver timeout to 2 seconds per try */
	_res.retry = 1;		/* only try once.. */

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (nlistf != NULL || memf != NULL)
		setgid(getgid());

	if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf)) == NULL)
		errx(1, "%s", errbuf);

	(void)time(&now);
	if ((ut = fopen(_PATH_UTMP, "r")) == NULL)
		err(1, "%s", _PATH_UTMP);

	if (*argv)
		sel_user = *argv;

	for (nusers = 0; fread(&utmp, sizeof(utmp), 1, ut);) {
		if (utmp.ut_name[0] == '\0')
			continue;
		++nusers;
		if (wcmd == 0 || (sel_user &&
		    strncmp(utmp.ut_name, sel_user, UT_NAMESIZE) != 0))
			continue;
		if ((ep = calloc(1, sizeof(struct entry))) == NULL)
			errx(1, "calloc");
		*nextp = ep;
		nextp = &(ep->next);
		memmove(&(ep->utmp), &utmp, sizeof(struct utmp));
		stp = ttystat(ep->utmp.ut_line);
		ep->tdev = stp->st_rdev;
#ifdef CPU_CONSDEV
		/*
		 * If this is the console device, attempt to ascertain
		 * the true console device dev_t.
		 */
		if (ep->tdev == 0) {
			int mib[2];
			size_t size;

			mib[0] = CTL_MACHDEP;
			mib[1] = CPU_CONSDEV;
			size = sizeof(dev_t);
			(void) sysctl(mib, 2, &ep->tdev, &size, NULL, 0);
		}
#endif
		if ((ep->idle = now - stp->st_atime) < 0)
			ep->idle = 0;
	}
	(void)fclose(ut);

	if (header || wcmd == 0) {
		pr_header(&now, nusers);
		if (wcmd == 0)
			exit (0);

#define HEADER	"USER             TTY FROM              LOGIN@  IDLE WHAT\n"
#define WUSED	(sizeof (HEADER) - sizeof ("WHAT\n"))
		(void)printf(HEADER);
	}

	if ((kp = kvm_getprocs(kd, KERN_PROC_ALL, 0, &nentries)) == NULL)
		err(1, "%s", kvm_geterr(kd));
	for (i = 0; i < nentries; i++, kp++) {
		struct proc *p = &kp->kp_proc;
		struct eproc *e;

		if (p->p_stat == SIDL || p->p_stat == SZOMB)
			continue;
		e = &kp->kp_eproc;
		for (ep = ehead; ep != NULL; ep = ep->next) {
			if (ep->tdev == e->e_tdev) {
				/*
				 * proc is associated with this terminal
				 */
				if (ep->kp == NULL && e->e_pgid == e->e_tpgid) {
					/*
					 * Proc is 'most interesting'
					 */
					if (proc_compare(&ep->kp->kp_proc, p))
						ep->kp = kp;
				}
				/*
				 * Proc debug option info; add to debug
				 * list using kinfo_proc kp_eproc.e_spare
				 * as next pointer; ptr to ptr avoids the
				 * ptr = long assumption.
				 */
				dkp = ep->dkp;
				ep->dkp = kp;
				*((struct kinfo_proc **)(&kp->kp_eproc.e_spare[ 0])) = dkp;
			}
		}
	}
	if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 &&
	     ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == -1 &&
	     ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) || ws.ws_col == 0)
	       ttywidth = 79;
        else
	       ttywidth = ws.ws_col - 1;
	argwidth = ttywidth - WUSED;
	if (argwidth < 4)
		argwidth = 8;
	for (ep = ehead; ep != NULL; ep = ep->next) {
		if (ep->kp == NULL) {
			ep->args = "-";
			continue;
		}
		ep->args = fmt_argv(kvm_getargv(kd, ep->kp, argwidth),
		    ep->kp->kp_proc.p_comm, MAXCOMLEN);
		if (ep->args == NULL)
			err(1, NULL);
	}
	/* sort by idle time */
	if (sortidle && ehead != NULL) {
		struct entry *from = ehead, *save;

		ehead = NULL;
		while (from != NULL) {
			for (nextp = &ehead;
			    (*nextp) && from->idle >= (*nextp)->idle;
			    nextp = &(*nextp)->next)
				continue;
			save = from;
			from = from->next;
			save->next = *nextp;
			*nextp = save;
		}
	}

	if (!nflag)
		if (gethostname(domain, sizeof(domain) - 1) < 0 ||
		    (p = strchr(domain, '.')) == 0)
			domain[0] = '\0';
		else {
			domain[sizeof(domain) - 1] = '\0';
			memmove(domain, p, strlen(p) + 1);
		}

	if ((vis_args = malloc(argwidth * 4 + 1)) == NULL)
		errx(1, "malloc");
	for (ep = ehead; ep != NULL; ep = ep->next) {
		p = *ep->utmp.ut_host ? ep->utmp.ut_host : "-";
		if ((x = strchr(p, ':')) != NULL)
			*x++ = '\0';
		if (!nflag && isdigit(*p) &&
		    (long)(l = inet_addr(p)) != -1 &&
		    (hp = gethostbyaddr((char *)&l, sizeof(l), AF_INET))) {
			if (domain[0] != '\0') {
				p = hp->h_name;
				p += strlen(hp->h_name);
				p -= strlen(domain);
				if (p > hp->h_name && strcmp(p, domain) == 0)
					*p = '\0';
			}
			p = hp->h_name;
		}
		if (nflag && *p && strcmp(p, "-") && inet_addr(p) == INADDR_NONE) {
			hp = gethostbyname(p);

			if (hp != NULL) {
				struct in_addr in;

				memmove(&in, hp->h_addr, sizeof(in));
				p = inet_ntoa(in);
			}
		}
		if (x) {
			(void)snprintf(buf, sizeof(buf), "%s:%.*s", p,
			    ep->utmp.ut_host + UT_HOSTSIZE - x, x);
			p = buf;
		}
		if( dflag) {
			for( dkp = ep->dkp; dkp != NULL; dkp = *((struct kinfo_proc **)(&dkp->kp_eproc.e_spare[ 0]))) {
				char *p;
				p = fmt_argv(kvm_getargv(kd, dkp, argwidth),
					    dkp->kp_proc.p_comm, MAXCOMLEN);
				if (p == NULL)
					p = "-";
				(void)printf( "\t\t%-9d %s\n", dkp->kp_proc.p_pid, p);
			}
		}
		(void)printf("%-*.*s %-3.3s %-*.*s ",
		    UT_NAMESIZE, UT_NAMESIZE, ep->utmp.ut_name,
		    strncmp(ep->utmp.ut_line, "tty", 3) &&
		    strncmp(ep->utmp.ut_line, "cua", 3) ?
		    ep->utmp.ut_line : ep->utmp.ut_line + 3,
		    UT_HOSTSIZE, UT_HOSTSIZE, *p ? p : "-");
		pr_attime(&ep->utmp.ut_time, &now);
		longidle=pr_idle(ep->idle);
		if (longidle)
			argwidth--;
		if (ep->args != NULL) {
			arglen = strlen(ep->args);
			strvisx(vis_args, ep->args,
			    arglen > argwidth ? argwidth : arglen,
			    VIS_TAB | VIS_NL | VIS_NOSLASH);
		}
		(void)printf("%.*s\n", argwidth, ep->args);
	}
	exit(0);
}

static void
pr_header(nowp, nusers)
	time_t *nowp;
	int nusers;
{
	double avenrun[3];
	time_t uptime;
	int days, hrs, i, mins;
	int mib[2];
	size_t size;
	char buf[256];

	/*
	 * Print time of day.
	 *
	 * SCCS forces the string manipulation below, as it replaces
	 * %, M, and % in a character string with the file name.
	 */
	(void)strftime(buf, sizeof(buf) - 1,
	    __CONCAT("%l:%","M%p"), localtime(nowp));
	buf[sizeof(buf) - 1] = '\0';
	(void)printf("%s ", buf);

	/*
	 * Print how long system has been up.
	 * (Found by looking getting "boottime" from the kernel)
	 */
	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	size = sizeof(boottime);
	if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 &&
	    boottime.tv_sec != 0) {
		uptime = now - boottime.tv_sec;
		uptime += 30;
		days = uptime / 86400;
		uptime %= 86400;
		hrs = uptime / 3600;
		uptime %= 3600;
		mins = uptime / 60;
		(void)printf(" up");
		if (days > 0)
			(void)printf(" %d day%s,", days, days > 1 ? "s" : "");
		if (hrs > 0 && mins > 0)
			(void)printf(" %2d:%02d,", hrs, mins);
		else {
			if (hrs > 0)
				(void)printf(" %d hr%s,",
				    hrs, hrs > 1 ? "s" : "");
			if (mins > 0)
				(void)printf(" %d min%s,",
				    mins, mins > 1 ? "s" : "");
		}
	}

	/* Print number of users logged in to system */
	(void)printf(" %d user%s", nusers, nusers == 1 ? "" : "s");

	/*
	 * Print 1, 5, and 15 minute load averages.
	 */
	if (getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0])) == -1)
		(void)printf(", no load average information available\n");
	else {
		(void)printf(", load averages:");
		for (i = 0; i < (sizeof(avenrun) / sizeof(avenrun[0])); i++) {
			if (i > 0)
				(void)printf(",");
			(void)printf(" %.2f", avenrun[i]);
		}
		(void)printf("\n");
	}
}

static struct stat *
ttystat(line)
	char *line;
{
	static struct stat sb;
	char ttybuf[MAXPATHLEN];

	(void)snprintf(ttybuf, sizeof(ttybuf), "%s/%s", _PATH_DEV, line);
	if (stat(ttybuf, &sb))
		err(1, "%s", ttybuf);
	return (&sb);
}

static void
usage(wcmd)
	int wcmd;
{
	if (wcmd)
		(void)fprintf(stderr,
		    "usage: w [-hin] [-M core] [-N system] [user]\n");
	else
		(void)fprintf(stderr,
			"usage: uptime\n");
	exit (1);
}
