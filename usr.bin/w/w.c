/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980, 1991 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)w.c	5.29 (Berkeley) 4/23/91";
#endif /* not lint */

/*
 * w - print system status (who and what)
 *
 * This program is similar to the systat command on Tenex/Tops 10/20
 *
 */
#include <sys/param.h>
#include <utmp.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <nlist.h>
#include <kvm.h>
#include <ctype.h>
#include <paths.h>
#include <string.h>
#include <stdio.h>

#ifdef SPPWAIT
#define NEWVM
#endif
#ifndef NEWVM
#include <machine/pte.h>
#include <sys/vm.h>
#endif

char	*program;
int	ttywidth;		/* width of tty */
int	argwidth;		/* width of tty */
int	header = 1;		/* true if -h flag: don't print heading */
int	wcmd = 1;		/* true if this is w(1), and not uptime(1) */
int	nusers;			/* number of users logged in now */
char *	sel_user;		/* login of particular user selected */
time_t	now;			/* the current time of day */
struct	timeval boottime;
time_t	uptime;			/* time of last reboot & elapsed time since */
struct	utmp utmp;
struct	winsize ws;
int	sortidle;		/* sort bu idle time */


/*
 * One of these per active utmp entry.  
 */
struct	entry {
	struct	entry *next;
	struct	utmp utmp;
	dev_t	tdev;		/* dev_t of terminal */
	int	idle;		/* idle time of terminal in minutes */
	struct	proc *proc;	/* list of procs in foreground */
	char	*args;		/* arg list of interesting process */
} *ep, *ehead = NULL, **nextp = &ehead;

struct nlist nl[] = {
	{ "_boottime" },
#define X_BOOTTIME	0
#if defined(hp300) || defined(i386)
	{ "_cn_tty" },
#define X_CNTTY		1
#endif
	{ "" },
};

#define USAGE "[ -hi ] [ user ]"
#define usage()	fprintf(stderr, "usage: %s: %s\n", program, USAGE)

main(argc, argv)
	int argc;
	char **argv;
{
	register int i;
	struct winsize win;
	register struct proc *p;
	struct eproc *e;
	struct stat *stp, *ttystat();
	FILE *ut;
	char *cp;
	int ch;
	extern char *optarg;
	extern int optind;
	char *attime();

	program = argv[0];
	/*
	 * are we w(1) or uptime(1)
	 */
	if ((cp = rindex(program, '/')) || *(cp = program) == '-')
		cp++;
	if (*cp == 'u')
		wcmd = 0;

	while ((ch = getopt(argc, argv, "hiflsuw")) != EOF)
		switch((char)ch) {
		case 'h':
			header = 0;
			break;
		case 'i':
			sortidle++;
			break;
		case 'f': case 'l': case 's': case 'u': case 'w':
			error("[-flsuw] no longer supported");
			usage();
			exit(1);
		case '?':
		default:
			usage();
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (*argv)
		sel_user = *argv;

	if (header && kvm_nlist(nl) != 0) {
		error("can't get namelist");
		exit (1);
	}
	time(&now);
	ut = fopen(_PATH_UTMP, "r");
	while (fread(&utmp, sizeof(utmp), 1, ut)) {
		if (utmp.ut_name[0] == '\0')
			continue;
		nusers++;
		if (wcmd == 0 || (sel_user && 
		    strncmp(utmp.ut_name, sel_user, UT_NAMESIZE) != 0))
			continue;
		if ((ep = (struct entry *)
		     calloc(1, sizeof (struct entry))) == NULL) {
			error("out of memory");
			exit(1);
		}
		*nextp = ep;
		nextp = &(ep->next);
		bcopy(&utmp, &(ep->utmp), sizeof (struct utmp));
		stp = ttystat(ep->utmp.ut_line);
		ep->tdev = stp->st_rdev;
#if defined(hp300) || defined(i386)
		/*
		 * XXX  If this is the console device, attempt to ascertain
		 * the true console device dev_t.
		 */
		if (ep->tdev == 0) {
			static dev_t cn_dev;

			if (nl[X_CNTTY].n_value) {
				struct tty cn_tty, *cn_ttyp;
				
				if (kvm_read((void *)nl[X_CNTTY].n_value,
				    &cn_ttyp, sizeof(cn_ttyp)) > 0) {
					(void)kvm_read(cn_ttyp, &cn_tty,
					    sizeof (cn_tty));
					cn_dev = cn_tty.t_dev;
				}
				nl[X_CNTTY].n_value = 0;
			}
			ep->tdev = cn_dev;
		}
#endif
		ep->idle = ((now - stp->st_atime) + 30) / 60; /* secs->mins */
		if (ep->idle < 0)
			ep->idle = 0;
	}
	fclose(ut);

	if (header || wcmd == 0) {
		double	avenrun[3];
		int days, hrs, mins;

		/*
		 * Print time of day 
		 */
		fputs(attime(&now), stdout);
		/*
		 * Print how long system has been up.
		 * (Found by looking for "boottime" in kernel)
		 */
		(void)kvm_read((void *)nl[X_BOOTTIME].n_value, &boottime, 
		    sizeof (boottime));
		uptime = now - boottime.tv_sec;
		uptime += 30;
		days = uptime / (60*60*24);
		uptime %= (60*60*24);
		hrs = uptime / (60*60);
		uptime %= (60*60);
		mins = uptime / 60;

		printf("  up");
		if (days > 0)
			printf(" %d day%s,", days, days>1?"s":"");
		if (hrs > 0 && mins > 0) {
			printf(" %2d:%02d,", hrs, mins);
		} else {
			if (hrs > 0)
				printf(" %d hr%s,", hrs, hrs>1?"s":"");
			if (mins > 0)
				printf(" %d min%s,", mins, mins>1?"s":"");
		}

		/* Print number of users logged in to system */
		printf("  %d user%s", nusers, nusers>1?"s":"");

		/*
		 * Print 1, 5, and 15 minute load averages.
		 */
		printf(",  load average:");
		(void)getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0]));
		for (i = 0; i < (sizeof(avenrun)/sizeof(avenrun[0])); i++) {
			if (i > 0)
				printf(",");
			printf(" %.2f", avenrun[i]);
		}
		printf("\n");
		if (wcmd == 0)		/* if uptime(1) then done */
			exit(0);
#define HEADER	"USER    TTY FROM              LOGIN@  IDLE WHAT\n"
#define WUSED	(sizeof (HEADER) - sizeof ("WHAT\n"))
		printf(HEADER);
	}

	while ((p = kvm_nextproc()) != NULL) {
		if (p->p_stat == SZOMB || (p->p_flag & SCTTY) == 0)
			continue;
		e = kvm_geteproc(p);
		for (ep = ehead; ep != NULL; ep = ep->next) {
			if (ep->tdev == e->e_tdev && e->e_pgid == e->e_tpgid) {
				/*
				 * Proc is in foreground of this terminal
				 */
				if (proc_compare(ep->proc, p))
					ep->proc = p;
				break;
			}
		}
	}
	if ((ioctl(1, TIOCGWINSZ, &ws) == -1 &&
	     ioctl(2, TIOCGWINSZ, &ws) == -1 &&
	     ioctl(0, TIOCGWINSZ, &ws) == -1) || ws.ws_col == 0)
	       ttywidth = 79;
        else
	       ttywidth = ws.ws_col - 1;
	argwidth = ttywidth - WUSED;
	if (argwidth < 4)
		argwidth = 8;
	for (ep = ehead; ep != NULL; ep = ep->next) {
		if(ep->proc != NULL) {
			ep->args = strdup(kvm_getargs(ep->proc, kvm_getu(ep->proc)));
			if (ep->args == NULL) {
				error("out of memory");
				exit(1);
			}
		}


	}
	/* sort by idle time */
	if (sortidle && ehead != NULL) {
		struct entry *from = ehead, *save;
		
		ehead = NULL;
		while (from != NULL) {
			for (nextp = &ehead; 
			    (*nextp) && from->idle >= (*nextp)->idle;
			    nextp = &(*nextp)->next)
				;
			save = from;
			from = from->next;
			save->next = *nextp;
			*nextp = save;
		}
	}
			
	for (ep = ehead; ep != NULL; ep = ep->next) {
		printf("%-*.*s %-2.2s %-*.*s %s",
			UT_NAMESIZE, UT_NAMESIZE, ep->utmp.ut_name,
			strncmp(ep->utmp.ut_line, "tty", 3) == 0 ? 
				ep->utmp.ut_line+3 : ep->utmp.ut_line,
			UT_HOSTSIZE, UT_HOSTSIZE, *ep->utmp.ut_host ?
				ep->utmp.ut_host : "-",
			attime(&ep->utmp.ut_time));
		if (ep->idle >= 36 * 60)
			printf(" %ddays ", (ep->idle + 12 * 60) / (24 * 60));
		else if (ep->idle == 0)
			printf("     - ");
		else
			prttime(ep->idle, " ");
		if(ep->args)
			printf("%.*s\n", argwidth, ep->args);
		else
			printf("-\n");
	}
	exit(0);
}

struct stat *
ttystat(line)
{
	static struct stat statbuf;
	char ttybuf[sizeof (_PATH_DEV) + UT_LINESIZE + 1];

	sprintf(ttybuf, "%s/%.*s", _PATH_DEV, UT_LINESIZE, line);
	(void) stat(ttybuf, &statbuf);

	return (&statbuf);
}

/*
 * prttime prints a time in hours and minutes or minutes and seconds.
 * The character string tail is printed at the end, obvious
 * strings to pass are "", " ", or "am".
 */
prttime(tim, tail)
	time_t tim;
	char *tail;
{

	if (tim >= 60) {
		printf(" %2d:", tim/60);
		tim %= 60;
		printf("%02d", tim);
	} else if (tim >= 0)
		printf("    %2d", tim);
	printf("%s", tail);
}

#include <varargs.h>

error(va_alist)
	va_dcl
{
	char *fmt;
	va_list ap;

	fprintf(stderr, "%s: ", program);
	va_start(ap);
	fmt = va_arg(ap, char *);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
