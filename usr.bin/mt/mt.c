/*
 * Copyright (c) 1980, 1993
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
static char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mt.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * mt --
 *   magnetic tape manipulation program
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

/* the appropriate sections of <sys/mtio.h> are also #ifdef'd for FreeBSD */
#if defined(__FreeBSD__)
/* c_flags */
#define NEED_2ARGS	0x01
#define ZERO_ALLOWED	0x02
#define IS_DENSITY	0x04
#define DISABLE_THIS	0x08
#endif /* defined(__FreeBSD__) */

struct commands {
	char *c_name;
	int c_code;
	int c_ronly;
#if defined(__FreeBSD__)
	int c_flags;
#endif /* defined(__FreeBSD__) */
} com[] = {
	{ "bsf",	MTBSF,	1 },
	{ "bsr",	MTBSR,	1 },
#if defined(__FreeBSD__)
	/* XXX FreeBSD considered "eof" dangerous, since it's being
	   confused with "eom" (and is an alias for "weof" anyway) */
	{ "eof",	MTWEOF, 0, DISABLE_THIS },
#else
	{ "eof",	MTWEOF, 0 },
#endif
	{ "fsf",	MTFSF,	1 },
	{ "fsr",	MTFSR,	1 },
	{ "offline",	MTOFFL,	1 },
	{ "rewind",	MTREW,	1 },
	{ "rewoffl",	MTOFFL,	1 },
	{ "status",	MTNOP,	1 },
	{ "weof",	MTWEOF,	0 },
#if defined(__FreeBSD__)
	{ "erase",	MTERASE, 0 },
	{ "blocksize",	MTSETBSIZ, 0, NEED_2ARGS|ZERO_ALLOWED },
	{ "density",	MTSETDNSTY, 0, NEED_2ARGS|ZERO_ALLOWED|IS_DENSITY },
	{ "eom",	MTEOD, 1 },
	{ "comp",	MTCOMP, 0, NEED_2ARGS|ZERO_ALLOWED },
	{ "retension",	MTRETENS, 1 },
#endif /* defined(__FreeBSD__) */
	{ NULL }
};

void err __P((const char *, ...));
void printreg __P((char *, u_int, char *));
void status __P((struct mtget *));
void usage __P((void));
#if defined (__FreeBSD__)
void st_status (struct mtget *);
int stringtodens (const char *s);
const char *denstostring (int d);
void warn_eof __P((void));
#endif /* defined (__FreeBSD__) */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register struct commands *comp;
	struct mtget mt_status;
	struct mtop mt_com;
	int ch, len, mtfd;
	char *p, *tape;

	if ((tape = getenv("TAPE")) == NULL)
		tape = DEFTAPE;

	while ((ch = getopt(argc, argv, "f:t:")) != EOF)
		switch(ch) {
		case 'f':
		case 't':
			tape = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1 || argc > 2)
		usage();

	len = strlen(p = *argv++);
	for (comp = com;; comp++) {
		if (comp->c_name == NULL)
			err("%s: unknown command", p);
		if (strncmp(p, comp->c_name, len) == 0)
			break;
	}
#if defined(__FreeBSD__)
	if((comp->c_flags & NEED_2ARGS) && argc != 2)
		usage();
	if(comp->c_flags & DISABLE_THIS) {
		warn_eof();
	}
#endif /* defined(__FreeBSD__) */
	if ((mtfd = open(tape, comp->c_ronly ? O_RDONLY : O_RDWR)) < 0)
		err("%s: %s", tape, strerror(errno));
	if (comp->c_code != MTNOP) {
		mt_com.mt_op = comp->c_code;
		if (*argv) {
#if defined (__FreeBSD__)
			if (!isdigit(**argv) &&
			    comp->c_flags & IS_DENSITY) {
				const char *dcanon;
				mt_com.mt_count = stringtodens(*argv);
				if (mt_com.mt_count == 0)
					err("%s: unknown density", *argv);
				dcanon = denstostring(mt_com.mt_count);
				if (strcmp(dcanon, *argv) != 0)
					printf(
					"Using \"%s\" as an alias for %s\n",
					       *argv, dcanon);
				p = "";
			} else
				/* allow for hex numbers; useful for density */
				mt_com.mt_count = strtol(*argv, &p, 0);
#else
			mt_com.mt_count = strtol(*argv, &p, 10);
#endif /* defined(__FreeBSD__) */
			if (mt_com.mt_count <=
#if defined (__FreeBSD__)
			    ((comp->c_flags & ZERO_ALLOWED)? -1: 0)
#else
			    0
#endif /* defined (__FreeBSD__) */
			    || *p)
				err("%s: illegal count", *argv);
		}
		else
			mt_com.mt_count = 1;
		if (ioctl(mtfd, MTIOCTOP, &mt_com) < 0)
			err("%s: %s: %s", tape, comp->c_name, strerror(errno));
	} else {
		if (ioctl(mtfd, MTIOCGET, &mt_status) < 0)
			err("%s", strerror(errno));
		status(&mt_status);
	}
	exit (0);
	/* NOTREACHED */
}

#ifdef vax
#include <vax/mba/mtreg.h>
#include <vax/mba/htreg.h>

#include <vax/uba/utreg.h>
#include <vax/uba/tmreg.h>
#undef b_repcnt		/* argh */
#include <vax/uba/tsreg.h>
#endif

#ifdef sun
#include <sundev/tmreg.h>
#include <sundev/arreg.h>
#endif

#ifdef tahoe
#include <tahoe/vba/cyreg.h>
#endif

struct tape_desc {
	short	t_type;		/* type of magtape device */
	char	*t_name;	/* printing name */
	char	*t_dsbits;	/* "drive status" register */
	char	*t_erbits;	/* "error" register */
} tapes[] = {
#ifdef vax
	{ MT_ISTS,	"ts11",		0,		TSXS0_BITS },
	{ MT_ISHT,	"tm03",		HTDS_BITS,	HTER_BITS },
	{ MT_ISTM,	"tm11",		0,		TMER_BITS },
	{ MT_ISMT,	"tu78",		MTDS_BITS,	0 },
	{ MT_ISUT,	"tu45",		UTDS_BITS,	UTER_BITS },
#endif
#ifdef sun
	{ MT_ISCPC,	"TapeMaster",	TMS_BITS,	0 },
	{ MT_ISAR,	"Archive",	ARCH_CTRL_BITS,	ARCH_BITS },
#endif
#ifdef tahoe
	{ MT_ISCY,	"cipher",	CYS_BITS,	CYCW_BITS },
#endif
#if defined (__FreeBSD__)
	/*
	 * XXX This is terrific.  The st driver reports the tape drive
	 * as 0x7 (MT_ISAR - Sun/Archive compatible); the wt driver
	 * either reports MT_ISVIPER1 for an Archive tape, or 0x11
	 * (MT_ISMFOUR) for other tapes.
	 * XXX for the wt driver, rely on it behaving like a "standard"
	 * magtape driver.
	 */
	{ MT_ISAR,	"SCSI tape drive", 0,		0 },
	{ MT_ISVIPER1,	"Archive Viper", 0,		0 },
	{ MT_ISMFOUR,	"Wangtek",	0,		0 },
#endif /* defined (__FreeBSD__) */
	{ 0 }
};

/*
 * Interpret the status buffer returned
 */
void
status(bp)
	register struct mtget *bp;
{
	register struct tape_desc *mt;

	for (mt = tapes;; mt++) {
		if (mt->t_type == 0) {
			(void)printf("%d: unknown tape drive type\n",
			    bp->mt_type);
			return;
		}
		if (mt->t_type == bp->mt_type)
			break;
	}
#if defined (__FreeBSD__)
	if(mt->t_type == MT_ISAR)
		st_status(bp);
	else {
#endif /* defined (__FreeBSD__) */
	(void)printf("%s tape drive, residual=%d\n", mt->t_name, bp->mt_resid);
	printreg("ds", bp->mt_dsreg, mt->t_dsbits);
	printreg("\ner", bp->mt_erreg, mt->t_erbits);
	(void)putchar('\n');
#if defined (__FreeBSD__)
	}
#endif /* defined (__FreeBSD__) */
}

/*
 * Print a register a la the %b format of the kernel's printf.
 */
void
printreg(s, v, bits)
	char *s;
	register u_int v;
	register char *bits;
{
	register int i, any = 0;
	register char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v);
	else
		printf("%s=%x", s, v);
	bits++;
	if (v && bits) {
		putchar('<');
		while (i = *bits++) {
			if (v & (1 << (i-1))) {
				if (any)
					putchar(',');
				any = 1;
				for (; (c = *bits) > 32; bits++)
					putchar(c);
			} else
				for (; *bits > 32; bits++)
					;
		}
		putchar('>');
	}
}

void
usage()
{
	(void)fprintf(stderr, "usage: mt [-f device] command [ count ]\n");
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
	(void)fprintf(stderr, "mt: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(1);
	/* NOTREACHED */
}

#if defined (__FreeBSD__)

struct densities {
	int dens;
	const char *name;
} dens [] = {
	{ 0x1,  "X3.22-1983" },
	{ 0x2,  "X3.39-1986" },
	{ 0x3,  "X3.54-1986" },
	{ 0x5,  "X3.136-1986" },
	{ 0x6,  "X3.157-1987" },
	{ 0x7,  "X3.116-1986" },
	{ 0x8,  "X3.158-1986" },
	{ 0x9,  "X3B5/87-099" },
	{ 0xA,  "X3B5/86-199" },
	{ 0xB,  "X3.56-1986" },
	{ 0xC,  "HI-TC1" },
	{ 0xD,  "HI-TC2" },
	{ 0xF,  "QIC-120" },
	{ 0x10, "QIC-150" },
	{ 0x11, "QIC-320" },
	{ 0x12, "QIC-1350" },
	{ 0x13, "X3B5/88-185A" },
	{ 0x14, "X3.202-1991" },
	{ 0x15, "ECMA TC17" },
	{ 0x16, "X3.193-1990" },
	{ 0x17, "X3B5/91-174" },
	{ 0, 0 }
};

const char *
denstostring(int d)
{
	static char buf[20];
	struct densities *sd;

	for (sd = dens; sd->dens; sd++)
		if (sd->dens == d)
			break;
	if (sd->dens == 0) {
		sprintf(buf, "0x%02x", d);
		return buf;
	} else
		return sd->name;
}

int
stringtodens(const char *s)
{
	struct densities *sd;
	size_t l = strlen(s);

	for (sd = dens; sd->dens; sd++)
		if (strncasecmp(sd->name, s, l) == 0)
			break;
	return sd->dens;
}


const char *
getblksiz(int bs)
{
	static char buf[25];
	if (bs == 0)
		return "variable";
	else {
		sprintf(buf, "= %d bytes", bs);
		return buf;
	}
}


void
st_status(struct mtget *bp)
{
	printf("Present Mode:   Density = %-12s Blocksize %s\n",
	       denstostring(bp->mt_density), getblksiz(bp->mt_blksiz));
	printf("---------available modes---------\n");
	printf("Mode 0:         Density = %-12s Blocksize %s\n",
	       denstostring(bp->mt_density0), getblksiz(bp->mt_blksiz0));
	printf("Mode 1:         Density = %-12s Blocksize %s\n",
	       denstostring(bp->mt_density1), getblksiz(bp->mt_blksiz1));
	printf("Mode 2:         Density = %-12s Blocksize %s\n",
	       denstostring(bp->mt_density2), getblksiz(bp->mt_blksiz2));
	printf("Mode 3:         Density = %-12s Blocksize %s\n",
	       denstostring(bp->mt_density3), getblksiz(bp->mt_blksiz3));
}

void
warn_eof(void)
{
	fprintf(stderr,
		"The \"eof\" command has been disabled.\n"
		"Use \"weof\" if you really want to write end-of-file marks,\n"
		"or \"eom\" if you rather want to skip to the end of "
		"recorded medium.\n");
	exit(1);
}

#endif /* defined (__FreeBSD__) */
