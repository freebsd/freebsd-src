/*
 * Copyright (c) 1980 The Regents of the University of California.
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
"@(#) Copyright (c) 1980 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mt.c	5.6 (Berkeley) 6/6/91";
#endif /* not lint */

/*
 * mt --
 *   magnetic tape manipulation program
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>

#define MTERA           -1
#define MTRET           -2

#define	equal(s1,s2)	(strcmp(s1, s2) == 0)

struct commands {
	char *c_name;
	int c_code;
	int c_ronly;
} com[] = {
	{ "weof",	MTWEOF,	0 },
	{ "eof",	MTWEOF,	0 },
	{ "fsf",	MTFSF,	1 },
	{ "bsf",	MTBSF,	1 },
	{ "fsr",	MTFSR,	1 },
	{ "bsr",	MTBSR,	1 },
	{ "rewind",	MTREW,	1 },
	{ "offline",	MTOFFL,	1 },
	{ "rewoffl",	MTOFFL,	1 },
	{ "status",	MTNOP,	1 },
	{ "erase",      MTERA,  0 },
	{ "retension",  MTRET,  1 },
	{ 0 }
};

int mtfd;
struct mtop mt_com;
struct mtget mt_status;
char *tape;

main(argc, argv)
	char **argv;
{
	char line[80], *getenv();
	register char *cp;
	register struct commands *comp;

	if (argc > 2 && (equal(argv[1], "-t") || equal(argv[1], "-f"))) {
		argc -= 2;
		tape = argv[2];
		argv += 2;
	} else
		if ((tape = getenv("TAPE")) == NULL)
			tape = DEFTAPE;
	if (argc < 2) {
usage:          fprintf(stderr, "Usage:\n");
		fprintf(stderr, "\tmt [ -f device ] command [ count ]\n");
		fprintf(stderr, "Commands:\n");
		fprintf(stderr, "\teof, weof - write tape mmrk\n");
		fprintf(stderr, "\tfsf       - seek forward for tape mark\n");
		fprintf(stderr, "\tbsf       - seek backward for tape mark\n");
		fprintf(stderr, "\tfsr       - seek record forward\n");
		fprintf(stderr, "\tbsr       - seek record backward\n");
		fprintf(stderr, "\trewind    - rewind the tape\n");
		fprintf(stderr, "\toffline   - rewind and unload the tape\n");
		fprintf(stderr, "\tstatus    - get tape status\n");
		fprintf(stderr, "\terase     - erase the tape\n");
		fprintf(stderr, "\tretension - retension the tape\n");
		exit(1);
	}
	cp = argv[1];
	for (comp = com; comp->c_name != NULL; comp++)
		if (strncmp(cp, comp->c_name, strlen(cp)) == 0)
			break;
	if (! comp->c_name)
		goto usage;
	if ((mtfd = open(tape, comp->c_ronly ? O_RDONLY : O_RDWR)) < 0) {
		perror(tape);
		exit(1);
	}
	switch (comp->c_code) {
	case MTERA:
		command(comp->c_code, "erase");
		break;
	case MTRET:
		command(comp->c_code, "retension");
		break;
	case MTNOP:
		if (ioctl(mtfd, MTIOCGET, (char *)&mt_status) < 0) {
			perror("mt");
			exit(2);
		}
		status(&mt_status);
		break;
	default:
		mt_com.mt_op = comp->c_code;
		mt_com.mt_count = (argc > 2 ? atoi(argv[2]) : 1);
		if (mt_com.mt_count < 0) {
			fprintf(stderr, "mt: negative repeat count\n");
			exit(1);
		}
		if (ioctl(mtfd, MTIOCTOP, &mt_com) < 0) {
			fprintf(stderr, "%s %s %d ", tape, comp->c_name,
				mt_com.mt_count);
			perror("failed");
			exit(2);
		}
		break;
	}
}

#ifdef vax
#include <vaxmba/mtreg.h>
#include <vaxmba/htreg.h>

#include <vaxuba/utreg.h>
#include <vaxuba/tmreg.h>
#undef b_repcnt		/* argh */
#include <vaxuba/tsreg.h>
#endif

#ifdef sun
#include <sundev/tmreg.h>
#include <sundev/arreg.h>
#endif

#ifdef tahoe
#include <tahoe/vba/cyreg.h>
#endif

#ifdef __386BSD__
#include <sys/../i386/isa/wtreg.h>
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
#ifdef __386BSD__
#ifdef WTDS_BITS
	{ MT_ISVIPER1,  "Archive",      WTDS_BITS,      WTER_BITS },
	{ 0x11,         "Wangtek",      WTDS_BITS,      WTER_BITS },
#endif
#endif
	{ 0 }
};

/*
 * Start erase or retension operation
 */

command(op, opname)
	char *opname;
{
#ifdef WTQICMD
	if (ioctl(mtfd, WTQICMD, op==MTERA ? QIC_ERASE : QIC_RETENS) >= 0)
		return;
	perror(opname);
#else
	fprintf(stderr, "%s %s: operation not supported\n", tape, opname);
#endif
}

/*
 * Interpret the status buffer returned
 */
status(bp)
	register struct mtget *bp;
{
	register struct tape_desc *mt;

	for (mt = tapes; mt->t_type; mt++)
		if (mt->t_type == bp->mt_type)
			break;
	if (mt->t_type == 0) {
		printf("unknown tape drive type (%d)\n", bp->mt_type);
		return;
	}
	printf("%s tape drive, residual=%d\n", mt->t_name, bp->mt_resid);
	printreg("ds", bp->mt_dsreg, mt->t_dsbits);
	printreg("\ner", bp->mt_erreg, mt->t_erbits);
	putchar('\n');
}

/*
 * Print a register a la the %b format of the kernel's printf
 */
printreg(s, v, bits)
	char *s;
	register char *bits;
	register unsigned short v;
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
