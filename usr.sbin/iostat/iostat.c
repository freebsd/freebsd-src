/*-
 * Copyright (c) 1986, 1991 The Regents of the University of California.
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
"@(#) Copyright (c) 1986, 1991 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)iostat.c	5.9 (Berkeley) 6/27/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <signal.h>
#include <fcntl.h>
#include <nlist.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <kvm.h>

struct nlist nl[] = {
#define	X_DK_TIME	0
	{ "_dk_time" },
#define	X_DK_XFER	1
	{ "_dk_xfer" },
#define	X_DK_WDS	2
	{ "_dk_wds" },
#define	X_TK_NIN	3
	{ "_tk_nin" },
#define	X_TK_NOUT	4
	{ "_tk_nout" },
#define	X_DK_SEEK	5
	{ "_dk_seek" },
#define	X_CP_TIME	6
	{ "_cp_time" },
#define	X_DK_WPMS	7
	{ "_dk_wpms" },
#define	X_HZ		8
	{ "_hz" },
#define	X_PHZ		9
	{ "_phz" },
#define	X_DK_NDRIVE	10
	{ "_dk_ndrive" },
#define	X_END		10
#ifdef hp300
#define	X_HPDINIT	(X_END+1)
	{ "_hp_dinit" },
#endif
#ifdef tahoe
#define	X_VBDINIT	(X_END+1)
	{ "_vbdinit" },
#endif
#ifdef vax
	{ "_mbdinit" },
#define X_MBDINIT	(X_END+1)
	{ "_ubdinit" },
#define X_UBDINIT	(X_END+2)
#endif
#ifdef __386BSD__
#define	X_ISA_BIO	(X_END+1)
	{ "_isa_devtab_bio" },
#endif /* __386BSD__ */
	{ NULL },
};

struct _disk {
	long	cp_time[CPUSTATES];
	long	*dk_time;
	long	*dk_wds;
	long	*dk_seek;
	long	*dk_xfer;
	long	tk_nin;
	long	tk_nout;
} cur, last;

double etime;
long *dk_wpms;
int dk_ndrive, *dr_select, hz, kmemfd, ndrives;
char **dr_name;

#define nlread(x, v) \
	kvm_read((void *)nl[x].n_value, (void *)&(v), sizeof(v))

#include "names.c"				/* XXX */

static void cpustats __P((void)), dkstats __P((void)), phdr __P((int));
static void usage __P((void)), err __P((const char *, ...));

main(argc, argv)
	int argc;
	char **argv;
{
	register int i;
	long tmp;
	int ch, hdrcnt, reps, interval, phz, ndrives;
	char **cp, *memfile, *namelist, buf[30];

	interval = reps = 0;
	namelist = memfile = NULL;
	while ((ch = getopt(argc, argv, "c:M:N:w:")) != EOF)
		switch(ch) {
		case 'c':
			reps = atoi(optarg);
			break;
		case 'M':
			memfile = optarg;
			break;
		case 'N':
			namelist = optarg;
			break;
		case 'w':
			interval = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (kvm_openfiles(namelist, memfile, NULL) == -1)
		err("kvm_openfiles: %s", kvm_geterr());
	if (kvm_nlist(nl) == -1)
		err("kvm_nlist: %s", kvm_geterr());
	if (nl[X_DK_NDRIVE].n_type == 0)
		err("dk_ndrive not found in namelist");
	(void)nlread(X_DK_NDRIVE, dk_ndrive);
	if (dk_ndrive <= 0)
		err("invalid dk_ndrive %d\n", dk_ndrive);

	cur.dk_time = calloc(dk_ndrive, sizeof(long));
	cur.dk_wds = calloc(dk_ndrive, sizeof(long));
	cur.dk_seek = calloc(dk_ndrive, sizeof(long));
	cur.dk_xfer = calloc(dk_ndrive, sizeof(long));
	last.dk_time = calloc(dk_ndrive, sizeof(long));
	last.dk_wds = calloc(dk_ndrive, sizeof(long));
	last.dk_seek = calloc(dk_ndrive, sizeof(long));
	last.dk_xfer = calloc(dk_ndrive, sizeof(long));
	dr_select = calloc(dk_ndrive, sizeof(int));
	dr_name = calloc(dk_ndrive, sizeof(char *));
	dk_wpms = calloc(dk_ndrive, sizeof(long));

	for (i = 0; i < dk_ndrive; i++) {
		(void)sprintf(buf, "dk%d", i);
		dr_name[i] = strdup(buf);
	}
	read_names();
	(void)nlread(X_HZ, hz);
	(void)nlread(X_PHZ, phz);
	if (phz)
		hz = phz;
	(void)kvm_read((void *)nl[X_DK_WPMS].n_value, dk_wpms,
		dk_ndrive * sizeof(dk_wpms));

	/*
	 * Choose drives to be displayed.  Priority goes to (in order) drives
	 * supplied as arguments and default drives.  If everything isn't
	 * filled in and there are drives not taken care of, display the first
	 * few that fit.
	 *
	 * The backward compatibility #ifdefs permit the syntax:
	 *	iostat [ drives ] [ interval [ count ] ]
	 */
#define	BACKWARD_COMPATIBILITY
	for (ndrives = 0; *argv; ++argv) {
#ifdef	BACKWARD_COMPATIBILITY
		if (isdigit(**argv))
			break;
#endif
		for (i = 0; i < dk_ndrive; i++) {
			if (strcmp(dr_name[i], *argv))
				continue;
			dr_select[i] = 1;
			++ndrives;
		}
	}
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		interval = atoi(*argv);
		if (*++argv)
			reps = atoi(*argv);
	}
#endif

	if (interval) {
		if (!reps)
			reps = -1;
	} else
		if (reps)
			interval = 1;

	for (i = 0; i < dk_ndrive && ndrives < 4; i++) {
		if (dr_select[i] || dk_wpms[i] == 0)
			continue;
		for (cp = defdrives; *cp; cp++)
			if (strcmp(dr_name[i], *cp) == 0) {
				dr_select[i] = 1;
				++ndrives;
				break;
			}
	}
	for (i = 0; i < dk_ndrive && ndrives < 4; i++) {
		if (dr_select[i])
			continue;
		dr_select[i] = 1;
		++ndrives;
	}

	(void)signal(SIGCONT, phdr);

	for (hdrcnt = 1;;) {
		if (!--hdrcnt) {
			phdr(0);
			hdrcnt = 20;
		}
		(void)kvm_read((void *)nl[X_DK_TIME].n_value,
		    cur.dk_time, dk_ndrive * sizeof(long));
		(void)kvm_read((void *)nl[X_DK_XFER].n_value,
		    cur.dk_xfer, dk_ndrive * sizeof(long));
		(void)kvm_read((void *)nl[X_DK_WDS].n_value,
		    cur.dk_wds, dk_ndrive * sizeof(long));
		(void)kvm_read((void *)nl[X_DK_SEEK].n_value,
		    cur.dk_seek, dk_ndrive * sizeof(long));
		(void)kvm_read((void *)nl[X_TK_NIN].n_value,
		    &cur.tk_nin, sizeof(cur.tk_nin));
		(void)kvm_read((void *)nl[X_TK_NOUT].n_value,
		    &cur.tk_nout, sizeof(cur.tk_nout));
		(void)kvm_read((void *)nl[X_CP_TIME].n_value,
		    cur.cp_time, sizeof(cur.cp_time));
		for (i = 0; i < dk_ndrive; i++) {
			if (!dr_select[i])
				continue;
#define X(fld)	tmp = cur.fld[i]; cur.fld[i] -= last.fld[i]; last.fld[i] = tmp
			X(dk_xfer);
			X(dk_seek);
			X(dk_wds);
			X(dk_time);
		}
		tmp = cur.tk_nin;
		cur.tk_nin -= last.tk_nin;
		last.tk_nin = tmp;
		tmp = cur.tk_nout;
		cur.tk_nout -= last.tk_nout;
		last.tk_nout = tmp;
		etime = 0;
		for (i = 0; i < CPUSTATES; i++) {
			X(cp_time);
			etime += cur.cp_time[i];
		}
		if (etime == 0.0)
			etime = 1.0;
		etime /= (float)hz;
		(void)printf("%4.0f%5.0f",
		    cur.tk_nin / etime, cur.tk_nout / etime);
		dkstats();
		cpustats();
		(void)printf("\n");
		(void)fflush(stdout);

		if (reps >= 0 && --reps <= 0)
			break;
		(void)sleep(interval);
	}
	exit(0);
}

/* ARGUSED */
void
phdr(notused)
	int notused;
{
	register int i;

	(void)printf("      tty");
	for (i = 0; i < dk_ndrive; i++)
		if (dr_select[i])
			(void)printf("          %3.3s ", dr_name[i]);
	(void)printf("         cpu\n tin tout");
	for (i = 0; i < dk_ndrive; i++)
		if (dr_select[i])
			(void)printf(" sps tps msps ");
	(void)printf(" us ni sy id\n");
}

void
dkstats()
{
	register int dn;
	double atime, itime, msps, words, xtime;

	for (dn = 0; dn < dk_ndrive; ++dn) {
		if (!dr_select[dn])
			continue;
		words = cur.dk_wds[dn] * 32;		/* words xfer'd */
		(void)printf("%4.0f",			/* sectors */
		    words / (DEV_BSIZE / 2) / etime);

		(void)printf("%4.0f", cur.dk_xfer[dn] / etime);

		if (dk_wpms[dn] && cur.dk_xfer[dn]) {
			atime = cur.dk_time[dn];	/* ticks disk busy */
			atime /= (float)hz;		/* ticks to seconds */
			xtime = words / dk_wpms[dn];	/* transfer time */
			itime = atime - xtime;		/* time not xfer'ing */
			if (itime < 0)
				msps = 0;
			else 
				msps = itime * 1000 / cur.dk_xfer[dn];
		} else
			msps = 0;
		(void)printf("%5.1f ", msps);
	}
}

void
cpustats()
{
	register int state;
	double time;

	time = 0;
	for (state = 0; state < CPUSTATES; ++state)
		time += cur.cp_time[state];
	for (state = 0; state < CPUSTATES; ++state)
		(void)printf("%3.0f",
		    100. * cur.cp_time[state] / (time ? time : 1));
}

void
usage()
{
	(void)fprintf(stderr,
"usage: iostat [-c count] [-M core] [-N system] [-w wait] [drives]\n");
	exit(1);
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
err(const char *fmt, ...)
#else
err(fmt, va_alist)
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "iostat: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(1);
	/* NOTREACHED */
}
