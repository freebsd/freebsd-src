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
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mt.c	8.2 (Berkeley) 5/4/95";
#endif
#endif /* not lint */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * mt --
 *   magnetic tape manipulation program
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* the appropriate sections of <sys/mtio.h> are also #ifdef'd for FreeBSD */
/* c_flags */
#define NEED_2ARGS	0x01
#define ZERO_ALLOWED	0x02
#define IS_DENSITY	0x04
#define DISABLE_THIS	0x08
#define IS_COMP		0x10

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

struct commands {
	char *c_name;
	int c_code;
	int c_ronly;
	int c_flags;
} com[] = {
	{ "bsf",	MTBSF,	1, 0 },
	{ "bsr",	MTBSR,	1, 0 },
	/* XXX FreeBSD considered "eof" dangerous, since it's being
	   confused with "eom" (and is an alias for "weof" anyway) */
	{ "eof",	MTWEOF, 0, DISABLE_THIS },
	{ "fsf",	MTFSF,	1, 0 },
	{ "fsr",	MTFSR,	1, 0 },
	{ "offline",	MTOFFL,	1, 0 },
	{ "rewind",	MTREW,	1, 0 },
	{ "rewoffl",	MTOFFL,	1, 0 },
	{ "status",	MTNOP,	1, 0 },
	{ "weof",	MTWEOF,	0, ZERO_ALLOWED },
	{ "erase",	MTERASE, 0, ZERO_ALLOWED},
	{ "blocksize",	MTSETBSIZ, 0, NEED_2ARGS|ZERO_ALLOWED },
	{ "density",	MTSETDNSTY, 0, NEED_2ARGS|ZERO_ALLOWED|IS_DENSITY },
	{ "eom",	MTEOD, 1, 0 },
	{ "eod",	MTEOD, 1, 0 },
	{ "smk",	MTWSS, 0, 0 },
	{ "wss",	MTWSS, 0, 0 },
	{ "fss",	MTFSS, 1, 0 },
	{ "bss",	MTBSS, 1, 0 },
	{ "comp",	MTCOMP, 0, NEED_2ARGS|ZERO_ALLOWED|IS_COMP },
	{ "retension",	MTRETENS, 1, 0 },
	{ "rdhpos",     MTIOCRDHPOS,  0, 0 },
	{ "rdspos",     MTIOCRDSPOS,  0, 0 },
	{ "sethpos",    MTIOCHLOCATE, 0, NEED_2ARGS|ZERO_ALLOWED },
	{ "setspos",    MTIOCSLOCATE, 0, NEED_2ARGS|ZERO_ALLOWED },
	{ "errstat",	MTIOCERRSTAT, 0, 0 },
	{ "setmodel",	MTIOCSETEOTMODEL, 0, NEED_2ARGS|ZERO_ALLOWED },
	{ "seteotmodel",	MTIOCSETEOTMODEL, 0, NEED_2ARGS|ZERO_ALLOWED },
	{ "getmodel",	MTIOCGETEOTMODEL, 0, 0 },
	{ "geteotmodel",	MTIOCGETEOTMODEL, 0, 0 },
	{ NULL, 0, 0, 0 }
};

const char *getblksiz(int);
void printreg(const char *, u_int, char *);
void status(struct mtget *);
void usage(void);
void st_status (struct mtget *);
int stringtodens (const char *s);
const char *denstostring (int d);
int denstobp(int d, int bpi);
u_int32_t stringtocomp(const char *s);
const char * comptostring(u_int32_t comp);
void warn_eof(void);

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

	while ((ch = getopt(argc, argv, "f:t:")) != -1)
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
			errx(1, "%s: unknown command", p);
		if (strncmp(p, comp->c_name, len) == 0)
			break;
	}
	if((comp->c_flags & NEED_2ARGS) && argc != 2)
		usage();
	if(comp->c_flags & DISABLE_THIS) {
		warn_eof();
	}
	if ((mtfd = open(tape, comp->c_ronly ? O_RDONLY : O_RDWR)) < 0)
		err(1, "%s", tape);
	if (comp->c_code != MTNOP) {
		mt_com.mt_op = comp->c_code;
		if (*argv) {
			if (!isdigit(**argv) &&
			    (comp->c_flags & IS_DENSITY)) {
				const char *dcanon;
				mt_com.mt_count = stringtodens(*argv);
				if (mt_com.mt_count == 0)
					errx(1, "%s: unknown density", *argv);
				dcanon = denstostring(mt_com.mt_count);
				if (strcmp(dcanon, *argv) != 0)
					printf(
					"Using \"%s\" as an alias for %s\n",
					       *argv, dcanon);
				p = "";
			} else if (!isdigit(**argv) &&
				   (comp->c_flags & IS_COMP)) {

				mt_com.mt_count = stringtocomp(*argv);
				if ((u_int32_t)mt_com.mt_count == 0xf0f0f0f0)
					errx(1, "%s: unknown compression",
					     *argv);
				p = "";
			} else
				/* allow for hex numbers; useful for density */
				mt_com.mt_count = strtol(*argv, &p, 0);
			if ((mt_com.mt_count <=
			    ((comp->c_flags & ZERO_ALLOWED)? -1: 0)
			    && ((comp->c_flags & IS_COMP) == 0)
			    ) || *p)
				errx(1, "%s: illegal count", *argv);
		}
		else
			mt_com.mt_count = 1;
		switch (comp->c_code) {
		case MTIOCERRSTAT:
		{
			unsigned int i;
			union mterrstat umn;
			struct scsi_tape_errors *s = &umn.scsi_errstat;

			if (ioctl(mtfd, comp->c_code, (caddr_t)&umn) < 0)
				err(2, "%s", tape);
			(void)printf("Last I/O Residual: %u\n", s->io_resid);
			(void)printf(" Last I/O Command:");
			for (i = 0; i < sizeof (s->io_cdb); i++)
				(void)printf(" %02X", s->io_cdb[i]);
			(void)printf("\n");
			(void)printf("   Last I/O Sense:\n\n\t");
			for (i = 0; i < sizeof (s->io_sense); i++) {
				(void)printf(" %02X", s->io_sense[i]);
				if (((i + 1) & 0xf) == 0) {
					(void)printf("\n\t");
				}
			}
			(void)printf("\n");
			(void)printf("Last Control Residual: %u\n",
			    s->ctl_resid);
			(void)printf(" Last Control Command:");
			for (i = 0; i < sizeof (s->ctl_cdb); i++)
				(void)printf(" %02X", s->ctl_cdb[i]);
			(void)printf("\n");
			(void)printf("   Last Control Sense:\n\n\t");
			for (i = 0; i < sizeof (s->ctl_sense); i++) {
				(void)printf(" %02X", s->ctl_sense[i]);
				if (((i + 1) & 0xf) == 0) {
					(void)printf("\n\t");
				}
			}
			(void)printf("\n\n");
			exit(0);
			/* NOTREACHED */
		}
		case MTIOCRDHPOS:
		case MTIOCRDSPOS:
		{
			u_int32_t block;
			if (ioctl(mtfd, comp->c_code, (caddr_t)&block) < 0)
				err(2, "%s", tape);
			(void)printf("%s: %s block location %u\n", tape,
			    (comp->c_code == MTIOCRDHPOS)? "hardware" :
			    "logical", block);
			exit(0);
			/* NOTREACHED */
		}
		case MTIOCSLOCATE:
		case MTIOCHLOCATE:
		{
			u_int32_t block = (u_int32_t)mt_com.mt_count;
			if (ioctl(mtfd, comp->c_code, (caddr_t)&block) < 0)
				err(2, "%s", tape);
			exit(0);
			/* NOTREACHED */
		}
		case MTIOCGETEOTMODEL:
		{
			u_int32_t om;
			if (ioctl(mtfd, MTIOCGETEOTMODEL, (caddr_t)&om) < 0)
				err(2, "%s", tape);
			(void)printf("%s: the model is %u filemar%s at EOT\n",
			    tape, om, (om > 1)? "ks" : "k");
			exit(0);
			/* NOTREACHED */
		}
		case MTIOCSETEOTMODEL:
		{
			u_int32_t om, nm = (u_int32_t)mt_com.mt_count;
			if (ioctl(mtfd, MTIOCGETEOTMODEL, (caddr_t)&om) < 0)
				err(2, "%s", tape);
			if (ioctl(mtfd, comp->c_code, (caddr_t)&nm) < 0)
				err(2, "%s", tape);
			(void)printf("%s: old model was %u filemar%s at EOT\n",
			    tape, om, (om > 1)? "ks" : "k");
			(void)printf("%s: new model  is %u filemar%s at EOT\n",
			    tape, nm, (nm > 1)? "ks" : "k");
			exit(0);
			/* NOTREACHED */
		}
		default:
			break;
		}
		if (ioctl(mtfd, MTIOCTOP, &mt_com) < 0)
			err(1, "%s: %s", tape, comp->c_name);
	} else {
		if (ioctl(mtfd, MTIOCGET, &mt_status) < 0)
			err(1, NULL);
		status(&mt_status);
	}
	exit(0);
	/* NOTREACHED */
}

struct tape_desc {
	short	t_type;		/* type of magtape device */
	char	*t_name;	/* printing name */
	char	*t_dsbits;	/* "drive status" register */
	char	*t_erbits;	/* "error" register */
} tapes[] = {
	{ MT_ISAR,	"SCSI tape drive", 0,		0 },
	{ 0, NULL, 0, 0 }
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
	if(mt->t_type == MT_ISAR)
		st_status(bp);
	else {
		(void)printf("%s tape drive, residual=%d\n", 
		    mt->t_name, bp->mt_resid);
		printreg("ds", (unsigned short)bp->mt_dsreg, mt->t_dsbits);
		printreg("\ner", (unsigned short)bp->mt_erreg, mt->t_erbits);
		(void)putchar('\n');
	}
}

/*
 * Print a register a la the %b format of the kernel's printf.
 */
void
printreg(s, v, bits)
	const char *s;
	register u_int v;
	register char *bits;
{
	register int i, any = 0;
	register char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v);
	else
		printf("%s=%x", s, v);
	if (!bits)
		return;
	bits++;
	if (v && bits) {
		putchar('<');
		while ((i = *bits++)) {
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
	(void)fprintf(stderr, "usage: mt [-f device] command [count]\n");
	exit(1);
}

struct densities {
	int dens;
	int bpmm;
	int bpi;
	const char *name;
} dens[] = {
	/*
	 * Taken from T10 Project 997D 
	 * SCSI-3 Stream Device Commands (SSC)
	 * Revision 11, 4-Nov-97
	 */
	/*Num.  bpmm    bpi     Reference     */
	{ 0x1,	32,	800,	"X3.22-1983" },
	{ 0x2,	63,	1600,	"X3.39-1986" },
	{ 0x3,	246,	6250,	"X3.54-1986" },
	{ 0x5,	315,	8000,	"X3.136-1986" },
	{ 0x6,	126,	3200,	"X3.157-1987" },
	{ 0x7,	252,	6400,	"X3.116-1986" },
	{ 0x8,	315,	8000,	"X3.158-1987" },
	{ 0x9,	491,	37871,	"X3.180" },
	{ 0xA,	262,	6667,	"X3B5/86-199" },
	{ 0xB,	63,	1600,	"X3.56-1986" },
	{ 0xC,	500,	12690,	"HI-TC1" },
	{ 0xD,	999,	25380,	"HI-TC2" },
	{ 0xF,	394,	10000,	"QIC-120" },
	{ 0x10,	394,	10000,	"QIC-150" },
	{ 0x11,	630,	16000,	"QIC-320" },
	{ 0x12,	2034,	51667,	"QIC-1350" },
	{ 0x13,	2400,	61000,	"X3B5/88-185A" },
	{ 0x14,	1703,	43245,	"X3.202-1991" },
	{ 0x15,	1789,	45434,	"ECMA TC17" },
	{ 0x16,	394,	10000,	"X3.193-1990" },
	{ 0x17,	1673,	42500,	"X3B5/91-174" },
	{ 0x18,	1673,	42500,	"X3B5/92-50" },
	{ 0x19, 2460,   62500,  "DLTapeIII" },
	{ 0x1A, 3214,   81633,  "DLTapeIV(20GB)" },
	{ 0x1B, 3383,   85937,  "DLTapeIV(35GB)" },
	{ 0x1C, 1654,	42000,	"QIC-385M" },
	{ 0x1D,	1512,	38400,	"QIC-410M" },
	{ 0x1E, 1385,	36000,	"QIC-1000C" },
	{ 0x1F,	2666,	67733,	"QIC-2100C" },
	{ 0x20, 2666,	67733,	"QIC-6GB(M)" },
	{ 0x21,	2666,	67733,	"QIC-20GB(C)" },
	{ 0x22,	1600,	40640,	"QIC-2GB(C)" },
	{ 0x23, 2666,	67733,	"QIC-875M" },
	{ 0x24,	2400,	61000,	"DDS-2" },
	{ 0x25,	3816,	97000,	"DDS-3" },
	{ 0x26,	3816,	97000,	"DDS-4" },
	{ 0x27,	3056,	77611,	"Mammoth" },
	{ 0x28,	1491,	37871,	"X3.224" },
	{ 0x41, 3868,   98250,  "DLTapeIV(40GB)" },
	{ 0x48, 5236,   133000, "SDLTapeI(110)" },
	{ 0x49, 7598,   193000, "SDLTapeI(160)" },
	{ 0, 0, 0, NULL }
};

struct compression_types {
	u_int32_t	comp_number;
	const char 	*name;
} comp_types[] = {
	{ 0x00, "none" },
	{ 0x00, "off" },
	{ 0x10, "IDRC" },
	{ 0x20, "DCLZ" },
	{ 0xffffffff, "enable" },
	{ 0xffffffff, "on" },
	{ 0xf0f0f0f0, NULL}
};

const char *
denstostring(int d)
{
	static char buf[20];
	struct densities *sd;

	/* densities 0 and 0x7f are handled as special cases */
	if (d == 0)
		return "default";
	if (d == 0x7f)
		return "same";
	for (sd = dens; sd->dens; sd++)
		if (sd->dens == d)
			break;
	if (sd->dens == 0)
		sprintf(buf, "0x%02x", d);
	else 
		sprintf(buf, "0x%02x:%s", d, sd->name);
	return buf;
}

/*
 * Given a specific density number, return either the bits per inch or bits
 * per millimeter for the given density.
 */
int
denstobp(int d, int bpi)
{
	struct densities *sd;

	for (sd = dens; sd->dens; sd++)
		if (sd->dens == d)
			break;
	if (sd->dens == 0)
		return(0);
	else {
		if (bpi)
			return(sd->bpi);
		else
			return(sd->bpmm);
	}
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
		sprintf(buf, "%d bytes", bs);
		return buf;
	}
}

const char *
comptostring(u_int32_t comp)
{
	static char buf[20];
	struct compression_types *ct;

	if (comp == MT_COMP_DISABLED)
		return "disabled";
	else if (comp == MT_COMP_UNSUPP)
		return "unsupported";

	for (ct = comp_types; ct->name; ct++)
		if (ct->comp_number == comp)
			break;

	if (ct->comp_number == 0xf0f0f0f0) {
		sprintf(buf, "0x%x", comp);
		return(buf);
	} else
		return(ct->name);
}

u_int32_t
stringtocomp(const char *s)
{
	struct compression_types *ct;
	size_t l = strlen(s);

	for (ct = comp_types; ct->name; ct++)
		if (strncasecmp(ct->name, s, l) == 0)
			break;

	return(ct->comp_number);
}

void
st_status(struct mtget *bp)
{
	printf("Mode      Density              Blocksize      bpi      "
	       "Compression\n"
	       "Current:  %-17s    %-12s   %-7d  %s\n"
	       "---------available modes---------\n"
	       "0:        %-17s    %-12s   %-7d  %s\n"
	       "1:        %-17s    %-12s   %-7d  %s\n"
	       "2:        %-17s    %-12s   %-7d  %s\n"
	       "3:        %-17s    %-12s   %-7d  %s\n",
	       denstostring(bp->mt_density), getblksiz(bp->mt_blksiz),
	       denstobp(bp->mt_density, TRUE), comptostring(bp->mt_comp),
	       denstostring(bp->mt_density0), getblksiz(bp->mt_blksiz0),
	       denstobp(bp->mt_density0, TRUE), comptostring(bp->mt_comp0),
	       denstostring(bp->mt_density1), getblksiz(bp->mt_blksiz1),
	       denstobp(bp->mt_density1, TRUE), comptostring(bp->mt_comp1),
	       denstostring(bp->mt_density2), getblksiz(bp->mt_blksiz2),
	       denstobp(bp->mt_density2, TRUE), comptostring(bp->mt_comp2),
	       denstostring(bp->mt_density3), getblksiz(bp->mt_blksiz3),
	       denstobp(bp->mt_density3, TRUE), comptostring(bp->mt_comp3));

	if (bp->mt_dsreg != MTIO_DSREG_NIL) {
		auto char foo[32];
		const char sfmt[] = "Current Driver State: %s.\n";
		printf("---------------------------------\n");
		switch (bp->mt_dsreg) {
		case MTIO_DSREG_REST:
			printf(sfmt, "at rest");      
			break;
		case MTIO_DSREG_RBSY:    
			printf(sfmt, "Communicating with drive");
			break;
		case MTIO_DSREG_WR:
			printf(sfmt, "Writing");
			break;
		case MTIO_DSREG_FMK:
			printf(sfmt, "Writing Filemarks");
			break;
		case MTIO_DSREG_ZER:
			printf(sfmt, "Erasing");
			break;
		case MTIO_DSREG_RD:
			printf(sfmt, "Reading");
			break;
		case MTIO_DSREG_FWD:
			printf(sfmt, "Spacing Forward");
			break;
		case MTIO_DSREG_REV:     
			printf(sfmt, "Spacing Reverse");
			break;
		case MTIO_DSREG_POS:
			printf(sfmt,
			    "Hardware Positioning (direction unknown)");
			break;
		case MTIO_DSREG_REW:
			printf(sfmt, "Rewinding");
			break;
		case MTIO_DSREG_TEN:
			printf(sfmt, "Retensioning");
			break;
		case MTIO_DSREG_UNL:
			printf(sfmt, "Unloading");
			break;
		case MTIO_DSREG_LD:
			printf(sfmt, "Loading");
			break;
		default:
			(void) sprintf(foo, "Unknown state 0x%x", bp->mt_dsreg);
			printf(sfmt, foo);
			break;
		}
	}
	if (bp->mt_resid == 0 && bp->mt_fileno == (daddr_t) -1 &&
	    bp->mt_blkno == (daddr_t) -1)
		return;
	printf("---------------------------------\n");
	printf("File Number: %d\tRecord Number: %d\tResidual Count %d\n",
	    bp->mt_fileno, bp->mt_blkno, bp->mt_resid);
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
