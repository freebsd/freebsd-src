/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1994 Theo de Raadt
 * All rights reserved.
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Symmetric Computer Systems.
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
 *      This product includes software developed by Theo de Raadt.
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
 *
 *	from: $NetBSD: disksubr.c,v 1.13 2000/12/17 22:39:18 pk $
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1987, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)disklabel.c	8.2 (Berkeley) 1/7/94";
/* from static char sccsid[] = "@(#)disklabel.c	1.2 (Symmetric) 11/28/85"; */
#endif
#endif /* not lint */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/disk.h>
#define DKTYPENAMES
#define FSTYPENAMES
#include <sys/disklabel.h>
#ifdef PC98
#include <sys/diskpc98.h>
#else
#include <sys/diskmbr.h>
#endif
#ifdef __sparc64__
#include <sys/sun_disklabel.h>
#endif

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>

#include "pathnames.h"

/*
 * Disklabel: read and write disklabels.
 * The label is usually placed on one of the first sectors of the disk.
 * Many machines also place a bootstrap in the same area,
 * in which case the label is embedded in the bootstrap.
 * The bootstrap source must leave space at the proper offset
 * for the label on such machines.
 */

#ifndef BBSIZE
#define	BBSIZE	8192			/* size of boot area, with label */
#endif

/* FIX!  These are too low, but are traditional */
#define DEFAULT_NEWFS_BLOCK  8192U
#define DEFAULT_NEWFS_FRAG   1024U
#define DEFAULT_NEWFS_CPG    16U

#define BIG_NEWFS_BLOCK  16384U
#define BIG_NEWFS_FRAG   2048U
#define BIG_NEWFS_CPG    64U

#if defined(__i386__) || defined(__ia64__)
#define	NUMBOOT	2
#elif defined(__alpha__) || defined(__sparc64__) || defined(__powerpc__)
#define	NUMBOOT	1
#else
#error	I do not know about this architecture.
#endif

void	makelabel(const char *, const char *, struct disklabel *);
int	writelabel(int, const char *, struct disklabel *);
void	l_perror(const char *);
struct disklabel *readlabel(int);
struct disklabel *makebootarea(char *, struct disklabel *, int);
void	display(FILE *, const struct disklabel *);
int	edit(struct disklabel *, int);
int	editit(void);
char	*skip(char *);
char	*word(char *);
int	getasciilabel(FILE *, struct disklabel *);
int	getasciipartspec(char *, struct disklabel *, int, int);
int	checklabel(struct disklabel *);
void	setbootflag(struct disklabel *);
void	Warning(const char *, ...) __printflike(1, 2);
void	usage(void);
struct disklabel *getvirginlabel(void);

#define	DEFEDITOR	_PATH_VI
#define	streq(a,b)	(strcmp(a,b) == 0)

char	*dkname;
char	*specname;
char	tmpfil[] = PATH_TMPFILE;

char	namebuf[BBSIZE], *np = namebuf;
struct	disklabel lab;
char	bootarea[BBSIZE];
char	blank[] = "";
char	unknown[] = "unknown";

#define MAX_PART ('z')
#define MAX_NUM_PARTS (1 + MAX_PART - 'a')
char    part_size_type[MAX_NUM_PARTS];
char    part_offset_type[MAX_NUM_PARTS];
int     part_set[MAX_NUM_PARTS];

#if NUMBOOT > 0
int	installboot;	/* non-zero if we should install a boot program */
char	*bootbuf;	/* pointer to buffer with remainder of boot prog */
int	bootsize;	/* size of remaining boot program */
char	*xxboot;	/* primary boot */
char	*bootxx;	/* secondary boot */
char	boot0[MAXPATHLEN];
char	boot1[MAXPATHLEN];
#endif

enum	{
	UNSPEC, EDIT, NOWRITE, READ, RESTORE, WRITE, WRITEABLE, WRITEBOOT
} op = UNSPEC;

int	rflag;
int	disable_write;   /* set to disable writing to disk label */

#define OPTIONS	"BNRWb:enrs:w"

int
main(int argc, char *argv[])
{
	struct disklabel *lp;
	FILE *t;
	int ch, f = 0, flag, error = 0;
	char *name = 0;

	while ((ch = getopt(argc, argv, OPTIONS)) != -1)
		switch (ch) {
#if NUMBOOT > 0
			case 'B':
				++installboot;
				break;
			case 'b':
				xxboot = optarg;
				break;
#if NUMBOOT > 1
			case 's':
				bootxx = optarg;
				break;
#endif
#endif
			case 'N':
				if (op != UNSPEC)
					usage();
				op = NOWRITE;
				break;
			case 'n':
				disable_write = 1;
				break;
			case 'R':
				if (op != UNSPEC)
					usage();
				op = RESTORE;
				break;
			case 'W':
				if (op != UNSPEC)
					usage();
				op = WRITEABLE;
				break;
			case 'e':
				if (op != UNSPEC)
					usage();
				op = EDIT;
				break;
			case 'r':
				++rflag;
				break;
			case 'w':
				if (op != UNSPEC)
					usage();
				op = WRITE;
				break;
			case '?':
			default:
				usage();
		}
	argc -= optind;
	argv += optind;
#if NUMBOOT > 0
	if (installboot) {
		rflag++;
		if (op == UNSPEC)
			op = WRITEBOOT;
	} else {
		if (op == UNSPEC)
			op = READ;
		xxboot = bootxx = 0;
	}
#else
	if (op == UNSPEC)
		op = READ;
#endif
	if (argc < 1)
		usage();

	dkname = argv[0];
	if (dkname[0] != '/') {
		(void)sprintf(np, "%s%s%c", _PATH_DEV, dkname, 'a' + RAW_PART);
		specname = np;
		np += strlen(specname) + 1;
	} else
		specname = dkname;
	f = open(specname, op == READ ? O_RDONLY : O_RDWR);
	if (f < 0 && errno == ENOENT && dkname[0] != '/') {
		(void)sprintf(specname, "%s%s", _PATH_DEV, dkname);
		np = namebuf + strlen(specname) + 1;
		f = open(specname, op == READ ? O_RDONLY : O_RDWR);
	}
	if (f < 0)
		err(4, "%s", specname);

	switch(op) {

	case UNSPEC:
		break;

	case EDIT:
		if (argc != 1)
			usage();
		lp = readlabel(f);
		error = edit(lp, f);
		break;

	case NOWRITE:
		flag = 0;
		if (ioctl(f, DIOCWLABEL, (char *)&flag) < 0)
			err(4, "ioctl DIOCWLABEL");
		break;

	case READ:
		if (argc != 1)
			usage();
		lp = readlabel(f);
		display(stdout, lp);
		error = checklabel(lp);
		break;

	case RESTORE:
#if NUMBOOT > 0
		if (installboot && argc == 3) {
			makelabel(argv[2], 0, &lab);
			argc--;

			/*
			 * We only called makelabel() for its side effect
			 * of setting the bootstrap file names.  Discard
			 * all changes to `lab' so that all values in the
			 * final label come from the ASCII label.
			 */
			bzero((char *)&lab, sizeof(lab));
		}
#endif
		if (argc != 2)
			usage();
		if (!(t = fopen(argv[1], "r")))
			err(4, "%s", argv[1]);
		if (!getasciilabel(t, &lab))
			exit(1);
		lp = makebootarea(bootarea, &lab, f);
		*lp = lab;
		error = writelabel(f, bootarea, lp);
		break;

	case WRITE:
		if (argc == 3) {
			name = argv[2];
			argc--;
		}
		if (argc != 2)
			usage();
		makelabel(argv[1], name, &lab);
		lp = makebootarea(bootarea, &lab, f);
		*lp = lab;
		if (checklabel(lp) == 0)
			error = writelabel(f, bootarea, lp);
		break;

	case WRITEABLE:
		flag = 1;
		if (ioctl(f, DIOCWLABEL, (char *)&flag) < 0)
			err(4, "ioctl DIOCWLABEL");
		break;

#if NUMBOOT > 0
	case WRITEBOOT:
	{
		struct disklabel tlab;

		lp = readlabel(f);
		tlab = *lp;
		if (argc == 2)
			makelabel(argv[1], 0, &lab);
		lp = makebootarea(bootarea, &lab, f);
		*lp = tlab;
		if (checklabel(lp) == 0)
			error = writelabel(f, bootarea, lp);
		break;
	}
#endif
	}
	exit(error);
}

/*
 * Construct a prototype disklabel from /etc/disktab.  As a side
 * effect, set the names of the primary and secondary boot files
 * if specified.
 */
void
makelabel(const char *type, const char *name, struct disklabel *lp)
{
	struct disklabel *dp;

	if (strcmp(type, "auto") == 0)
		dp = getvirginlabel();
	else
		dp = getdiskbyname(type);
	if (dp == NULL)
		errx(1, "%s: unknown disk type", type);
	*lp = *dp;
	bzero(lp->d_packname, sizeof(lp->d_packname));
	if (name)
		(void)strncpy(lp->d_packname, name, sizeof(lp->d_packname));
}

int
writelabel(int f, const char *boot, struct disklabel *lp)
{
	int flag;
#ifdef __alpha__
	u_long *p, sum;
	int i;
#endif
#ifdef __sparc64__
	struct sun_disklabel *sl;
	u_short cksum, *sp1, *sp2;
	struct partition *npp;
	struct sun_dkpart *spp;
	int i, secpercyl;
#endif

	if (disable_write) {
		Warning("write to disk label supressed - label was as follows:");
		display(stdout, lp);
		return (0);
	} else {
		setbootflag(lp);
		lp->d_magic = DISKMAGIC;
		lp->d_magic2 = DISKMAGIC;
		lp->d_checksum = 0;
		lp->d_checksum = dkcksum(lp);
		if (rflag) {
			/*
			 * First set the kernel disk label,
			 * then write a label to the raw disk.
			 * If the SDINFO ioctl fails because it is unimplemented,
			 * keep going; otherwise, the kernel consistency checks
			 * may prevent us from changing the current (in-core)
			 * label.
			 */
			if (ioctl(f, DIOCSDINFO, lp) < 0 &&
				errno != ENODEV && errno != ENOTTY) {
				l_perror("ioctl DIOCSDINFO");
				return (1);
			}
			(void)lseek(f, (off_t)0, SEEK_SET);
			
#ifdef __alpha__
			/*
			 * Generate the bootblock checksum for the SRM console.
			 */
			for (p = (u_long *)boot, i = 0, sum = 0; i < 63; i++)
				sum += p[i];
			p[63] = sum;
#endif
#ifdef __sparc64__
			/*
			 * Generate a Sun disklabel around the BSD label for
			 * PROM compatability.
			 */
			sl = (struct sun_disklabel *)boot;
			memcpy(sl->sl_text, lp->d_packname, sizeof(lp->d_packname));
			sl->sl_rpm = lp->d_rpm;
			sl->sl_pcylinders = lp->d_ncylinders +
			    lp->d_acylinders; /* XXX */
			sl->sl_sparespercyl = lp->d_sparespercyl;
			sl->sl_interleave = lp->d_interleave;
			sl->sl_ncylinders = lp->d_ncylinders;
			sl->sl_acylinders = lp->d_acylinders;
			sl->sl_ntracks = lp->d_ntracks;
			sl->sl_nsectors = lp->d_nsectors;
			sl->sl_magic = SUN_DKMAGIC;
			secpercyl = sl->sl_nsectors * sl->sl_ntracks;
			for (i = 0; i < 8; i++) {
				spp = &sl->sl_part[i];
				npp = &lp->d_partitions[i];
				/*
				 * SunOS partitions must start on a cylinder
				 * boundary. Note this restriction is forced
				 * upon FreeBSD/sparc64 labels too, since we
				 * want to keep both labels synchronised.
				 */
				spp->sdkp_cyloffset = npp->p_offset / secpercyl;
				spp->sdkp_nsectors = npp->p_size;
			}

			/* Compute the XOR checksum. */
			sp1 = (u_short *)sl;
			sp2 = (u_short *)(sl + 1);
			sl->sl_cksum = cksum = 0;
			while (sp1 < sp2)
				cksum ^= *sp1++;
			sl->sl_cksum = cksum;
#endif
			/*
			 * write enable label sector before write (if necessary),
			 * disable after writing.
			 */
			flag = 1;
			(void)ioctl(f, DIOCWLABEL, &flag);
			if (write(f, boot, lp->d_bbsize) != (int)lp->d_bbsize) {
				warn("write");
				return (1);
			}
#if NUMBOOT > 0
			/*
			 * Output the remainder of the disklabel
			 */
			if (bootbuf && write(f, bootbuf, bootsize) != bootsize) {
				warn("write");
				return(1);
			}
#endif
			flag = 0;
			(void) ioctl(f, DIOCWLABEL, &flag);
		} else if (ioctl(f, DIOCWDINFO, lp) < 0) {
			l_perror("ioctl DIOCWDINFO");
			return (1);
		}
	}
	return (0);
}

void
l_perror(const char *s)
{
	switch (errno) {

	case ESRCH:
		warnx("%s: no disk label on disk;", s);
		fprintf(stderr, "add \"-r\" to install initial label\n");
		break;

	case EINVAL:
		warnx("%s: label magic number or checksum is wrong!", s);
		fprintf(stderr, "(disklabel or kernel is out of date?)\n");
		break;

	case EBUSY:
		warnx("%s: open partition would move or shrink", s);
		break;

	case EXDEV:
		warnx("%s: '%c' partition must start at beginning of disk",
		    s, 'a' + RAW_PART);
		break;

	default:
		warn((char *)NULL);
		break;
	}
}

/*
 * Fetch disklabel for disk.
 * Use ioctl to get label unless -r flag is given.
 */
struct disklabel *
readlabel(int f)
{
	struct disklabel *lp;

	if (rflag) {
		if (read(f, bootarea, BBSIZE) < BBSIZE)
			err(4, "%s", specname);
		for (lp = (struct disklabel *)bootarea;
		    lp <= (struct disklabel *)(bootarea + BBSIZE - sizeof(*lp));
		    lp = (struct disklabel *)((char *)lp + 16))
			if (lp->d_magic == DISKMAGIC &&
			    lp->d_magic2 == DISKMAGIC)
				break;
		if (lp > (struct disklabel *)(bootarea+BBSIZE-sizeof(*lp)) ||
		    lp->d_magic != DISKMAGIC || lp->d_magic2 != DISKMAGIC ||
		    dkcksum(lp) != 0)
			errx(1,
	    "bad pack magic number (label is damaged, or pack is unlabeled)");
	} else {
		lp = &lab;
		if (ioctl(f, DIOCGDINFO, lp) < 0)
			err(4, "ioctl DIOCGDINFO");
	}
	return (lp);
}

/*
 * Construct a bootarea (d_bbsize bytes) in the specified buffer ``boot''
 * Returns a pointer to the disklabel portion of the bootarea.
 */
struct disklabel *
makebootarea(char *boot, struct disklabel *dp, int f)
{
	struct disklabel *lp;
	char *p;
	int b;
#if NUMBOOT > 0
	char *dkbasename;
	struct stat sb;
#endif
#ifdef __alpha__
	u_long *bootinfo;
	int n;
#endif
#ifdef __i386__
	char *tmpbuf;
	int i, found;
#endif

	/* XXX */
	if (dp->d_secsize == 0) {
		dp->d_secsize = DEV_BSIZE;
		dp->d_bbsize = BBSIZE;
	}
	lp = (struct disklabel *)
		(boot + (LABELSECTOR * dp->d_secsize) + LABELOFFSET);
	bzero((char *)lp, sizeof *lp);
#if NUMBOOT > 0
	/*
	 * If we are not installing a boot program but we are installing a
	 * label on disk then we must read the current bootarea so we don't
	 * clobber the existing boot.
	 */
	if (!installboot) {
		if (rflag) {
			if (read(f, boot, BBSIZE) < BBSIZE)
				err(4, "%s", specname);
			bzero((char *)lp, sizeof *lp);
		}
		return (lp);
	}
	/*
	 * We are installing a boot program.  Determine the name(s) and
	 * read them into the appropriate places in the boot area.
	 */
	if (!xxboot || !bootxx) {
		dkbasename = np;
		if ((p = rindex(dkname, '/')) == NULL)
			p = dkname;
		else
			p++;
		while (*p && !isdigit(*p))
			*np++ = *p++;
		*np++ = '\0';

		if (!xxboot) {
			(void)sprintf(boot0, "%s/boot1", _PATH_BOOTDIR);
			xxboot = boot0;
		}
#if NUMBOOT > 1
		if (!bootxx) {
			(void)sprintf(boot1, "%s/boot2", _PATH_BOOTDIR);
			bootxx = boot1;
		}
#endif
	}

	/*
	 * Strange rules:
	 * 1. One-piece bootstrap (hp300/hp800)
	 * 1. One-piece bootstrap (alpha/sparc64)
	 *	up to d_bbsize bytes of ``xxboot'' go in bootarea, the rest
	 *	is remembered and written later following the bootarea.
	 * 2. Two-piece bootstraps (i386/ia64)
	 *	up to d_secsize bytes of ``xxboot'' go in first d_secsize
	 *	bytes of bootarea, remaining d_bbsize-d_secsize filled
	 *	from ``bootxx''.
	 */
	b = open(xxboot, O_RDONLY);
	if (b < 0)
		err(4, "%s", xxboot);
#if NUMBOOT > 1
#ifdef __i386__
	/*
	 * XXX Botch alert.
	 * The i386 has the so-called fdisk table embedded into the
	 * primary bootstrap.  We take care to not clobber it, but
	 * only if it does already contain some data.  (Otherwise,
	 * the xxboot provides a template.)
	 */
	if ((tmpbuf = (char *)malloc((int)dp->d_secsize)) == 0)
		err(4, "%s", xxboot);
	memcpy((void *)tmpbuf, (void *)boot, (int)dp->d_secsize);
#endif /* __i386__ */
	if (read(b, boot, (int)dp->d_secsize) < 0)
		err(4, "%s", xxboot);
	(void)close(b);
#ifdef PC98
	for (i = DOSPARTOFF, found = 0;
	     !found && i < (int)(DOSPARTOFF + NDOSPART * sizeof(struct pc98_partition));
	     i++)
		found = tmpbuf[i] != 0;
	if (found)
		memcpy((void *)&boot[DOSPARTOFF],
		       (void *)&tmpbuf[DOSPARTOFF],
		       NDOSPART * sizeof(struct pc98_partition));
	free(tmpbuf);
#elif defined(__i386__)
	for (i = DOSPARTOFF, found = 0;
	     !found && i < (int)(DOSPARTOFF + NDOSPART*sizeof(struct dos_partition));
	     i++)
		found = tmpbuf[i] != 0;
	if (found)
		memcpy((void *)&boot[DOSPARTOFF],
		       (void *)&tmpbuf[DOSPARTOFF],
		       NDOSPART * sizeof(struct dos_partition));
	free(tmpbuf);
#endif /* __i386__ */
	b = open(bootxx, O_RDONLY);
	if (b < 0)
		err(4, "%s", bootxx);
	if (fstat(b, &sb) != 0)
		err(4, "%s", bootxx);
	if (dp->d_secsize + sb.st_size > dp->d_bbsize)
		errx(4, "%s too large", bootxx);
	if (read(b, &boot[dp->d_secsize],
		 (int)(dp->d_bbsize-dp->d_secsize)) < 0)
		err(4, "%s", bootxx);
#else /* !(NUMBOOT > 1) */
#ifdef __alpha__
	/*
	 * On the alpha, the primary bootstrap starts at the
	 * second sector of the boot area.  The first sector
	 * contains the label and must be edited to contain the
	 * size and location of the primary bootstrap.
	 */
	n = read(b, boot + dp->d_secsize, (int)dp->d_bbsize);
	if (n < 0)
		err(4, "%s", xxboot);
	bootinfo = (u_long *)(boot + 480);
	bootinfo[0] = (n + dp->d_secsize - 1) / dp->d_secsize;
	bootinfo[1] = 1;	/* start at sector 1 */
	bootinfo[2] = 0;	/* flags (must be zero) */
#else /* !__alpha__ */
	if (read(b, boot, (int)dp->d_bbsize) < 0)
		err(4, "%s", xxboot);
#endif /* __alpha__ */
	if (fstat(b, &sb) != 0)
		err(4, "%s", xxboot);
	bootsize = (int)sb.st_size - dp->d_bbsize;
	if (bootsize > 0) {
		/* XXX assume d_secsize is a power of two */
		bootsize = (bootsize + dp->d_secsize-1) & ~(dp->d_secsize-1);
		bootbuf = (char *)malloc((size_t)bootsize);
		if (bootbuf == 0)
			err(4, "%s", xxboot);
		if (read(b, bootbuf, bootsize) < 0) {
			free(bootbuf);
			err(4, "%s", xxboot);
		}
	}
#endif /* NUMBOOT > 1 */
	(void)close(b);
#endif /* NUMBOOT > 0 */
	/*
	 * Make sure no part of the bootstrap is written in the area
	 * reserved for the label.
	 */
	for (p = (char *)lp; p < (char *)lp + sizeof(struct disklabel); p++)
		if (*p)
			errx(2, "bootstrap doesn't leave room for disk label");
	return (lp);
}

void
display(FILE *f, const struct disklabel *lp)
{
	int i, j;
	const struct partition *pp;

	fprintf(f, "# %s:\n", specname);
	if (lp->d_type < DKMAXTYPES)
		fprintf(f, "type: %s\n", dktypenames[lp->d_type]);
	else
		fprintf(f, "type: %u\n", lp->d_type);
	fprintf(f, "disk: %.*s\n", (int)sizeof(lp->d_typename),
		lp->d_typename);
	fprintf(f, "label: %.*s\n", (int)sizeof(lp->d_packname),
		lp->d_packname);
	fprintf(f, "flags:");
	if (lp->d_flags & D_REMOVABLE)
		fprintf(f, " removeable");
	if (lp->d_flags & D_ECC)
		fprintf(f, " ecc");
	if (lp->d_flags & D_BADSECT)
		fprintf(f, " badsect");
	fprintf(f, "\n");
	fprintf(f, "bytes/sector: %lu\n", (u_long)lp->d_secsize);
	fprintf(f, "sectors/track: %lu\n", (u_long)lp->d_nsectors);
	fprintf(f, "tracks/cylinder: %lu\n", (u_long)lp->d_ntracks);
	fprintf(f, "sectors/cylinder: %lu\n", (u_long)lp->d_secpercyl);
	fprintf(f, "cylinders: %lu\n", (u_long)lp->d_ncylinders);
	fprintf(f, "sectors/unit: %lu\n", (u_long)lp->d_secperunit);
	fprintf(f, "rpm: %u\n", lp->d_rpm);
	fprintf(f, "interleave: %u\n", lp->d_interleave);
	fprintf(f, "trackskew: %u\n", lp->d_trackskew);
	fprintf(f, "cylinderskew: %u\n", lp->d_cylskew);
	fprintf(f, "headswitch: %lu\t\t# milliseconds\n",
	    (u_long)lp->d_headswitch);
	fprintf(f, "track-to-track seek: %ld\t# milliseconds\n",
	    (u_long)lp->d_trkseek);
	fprintf(f, "drivedata: ");
	for (i = NDDATA - 1; i >= 0; i--)
		if (lp->d_drivedata[i])
			break;
	if (i < 0)
		i = 0;
	for (j = 0; j <= i; j++)
		fprintf(f, "%lu ", (u_long)lp->d_drivedata[j]);
	fprintf(f, "\n\n%u partitions:\n", lp->d_npartitions);
	fprintf(f,
	    "#        size   offset    fstype   [fsize bsize bps/cpg]\n");
	pp = lp->d_partitions;
	for (i = 0; i < lp->d_npartitions; i++, pp++) {
		if (pp->p_size) {
			fprintf(f, "  %c: %8lu %8lu  ", 'a' + i,
			   (u_long)pp->p_size, (u_long)pp->p_offset);
			if (pp->p_fstype < FSMAXTYPES)
				fprintf(f, "%8.8s", fstypenames[pp->p_fstype]);
			else
				fprintf(f, "%8d", pp->p_fstype);
			switch (pp->p_fstype) {

			case FS_UNUSED:				/* XXX */
				fprintf(f, "    %5lu %5lu %5.5s ",
				    (u_long)pp->p_fsize,
				    (u_long)(pp->p_fsize * pp->p_frag), "");
				break;

			case FS_BSDFFS:
				fprintf(f, "    %5lu %5lu %5u ",
				    (u_long)pp->p_fsize,
				    (u_long)(pp->p_fsize * pp->p_frag),
				    pp->p_cpg);
				break;

			case FS_BSDLFS:
				fprintf(f, "    %5lu %5lu %5d",
				    (u_long)pp->p_fsize,
				    (u_long)(pp->p_fsize * pp->p_frag),
				    pp->p_cpg);
				break;

			default:
				fprintf(f, "%20.20s", "");
				break;
			}
			fprintf(f, "\t# (Cyl. %4lu",
			    (u_long)(pp->p_offset / lp->d_secpercyl));
			if (pp->p_offset % lp->d_secpercyl)
			    putc('*', f);
			else
			    putc(' ', f);
			fprintf(f, "- %lu",
			    (u_long)((pp->p_offset + pp->p_size +
			    lp->d_secpercyl - 1) /
			    lp->d_secpercyl - 1));
			if (pp->p_size % lp->d_secpercyl)
			    putc('*', f);
			fprintf(f, ")\n");
		}
	}
	fflush(f);
}

int
edit(struct disklabel *lp, int f)
{
	int c, fd;
	struct disklabel label;
	FILE *fp;

	if ((fd = mkstemp(tmpfil)) == -1 ||
	    (fp = fdopen(fd, "w")) == NULL) {
		warnx("can't create %s", tmpfil);
		return (1);
	}
	display(fp, lp);
	fclose(fp);
	for (;;) {
		if (!editit())
			break;
		fp = fopen(tmpfil, "r");
		if (fp == NULL) {
			warnx("can't reopen %s for reading", tmpfil);
			break;
		}
		bzero((char *)&label, sizeof(label));
		if (getasciilabel(fp, &label)) {
			*lp = label;
			if (writelabel(f, bootarea, lp) == 0) {
				fclose(fp);
				(void) unlink(tmpfil);
				return (0);
			}
		}
		fclose(fp);
		printf("re-edit the label? [y]: "); fflush(stdout);
		c = getchar();
		if (c != EOF && c != (int)'\n')
			while (getchar() != (int)'\n')
				;
		if  (c == (int)'n')
			break;
	}
	(void) unlink(tmpfil);
	return (1);
}

int
editit(void)
{
	int pid, xpid;
	int locstat, omask;
	const char *ed;

	omask = sigblock(sigmask(SIGINT)|sigmask(SIGQUIT)|sigmask(SIGHUP));
	while ((pid = fork()) < 0) {
		if (errno == EPROCLIM) {
			warnx("you have too many processes");
			return(0);
		}
		if (errno != EAGAIN) {
			warn("fork");
			return(0);
		}
		sleep(1);
	}
	if (pid == 0) {
		sigsetmask(omask);
		setgid(getgid());
		setuid(getuid());
		if ((ed = getenv("EDITOR")) == (char *)0)
			ed = DEFEDITOR;
		execlp(ed, ed, tmpfil, (char *)0);
		err(1, "%s", ed);
	}
	while ((xpid = wait(&locstat)) >= 0)
		if (xpid == pid)
			break;
	sigsetmask(omask);
	return(!locstat);
}

char *
skip(char *cp)
{

	while (*cp != '\0' && isspace(*cp))
		cp++;
	if (*cp == '\0' || *cp == '#')
		return (NULL);
	return (cp);
}

char *
word(char *cp)
{
	char c;

	while (*cp != '\0' && !isspace(*cp) && *cp != '#')
		cp++;
	if ((c = *cp) != '\0') {
		*cp++ = '\0';
		if (c != '#')
			return (skip(cp));
	}
	return (NULL);
}

/*
 * Read an ascii label in from fd f,
 * in the same format as that put out by display(),
 * and fill in lp.
 */
int
getasciilabel(FILE *f, struct disklabel *lp)
{
	char *cp;
	const char **cpp;
	u_int part;
	char *tp, line[BUFSIZ];
	u_long v;
	int lineno = 0, errors = 0;
	int i;

	bzero(&part_set, sizeof(part_set));
	bzero(&part_size_type, sizeof(part_size_type));
	bzero(&part_offset_type, sizeof(part_offset_type));
	lp->d_bbsize = BBSIZE;				/* XXX */
	lp->d_sbsize = 0;				/* XXX */
	while (fgets(line, sizeof(line) - 1, f)) {
		lineno++;
		if ((cp = index(line,'\n')) != 0)
			*cp = '\0';
		cp = skip(line);
		if (cp == NULL)
			continue;
		tp = index(cp, ':');
		if (tp == NULL) {
			fprintf(stderr, "line %d: syntax error\n", lineno);
			errors++;
			continue;
		}
		*tp++ = '\0', tp = skip(tp);
		if (streq(cp, "type")) {
			if (tp == NULL)
				tp = unknown;
			cpp = dktypenames;
			for (; cpp < &dktypenames[DKMAXTYPES]; cpp++)
				if (*cpp && streq(*cpp, tp)) {
					lp->d_type = cpp - dktypenames;
					break;
				}
			if (cpp < &dktypenames[DKMAXTYPES])
				continue;
			v = strtoul(tp, NULL, 10);
			if (v >= DKMAXTYPES)
				fprintf(stderr, "line %d:%s %lu\n", lineno,
				    "Warning, unknown disk type", v);
			lp->d_type = v;
			continue;
		}
		if (streq(cp, "flags")) {
			for (v = 0; (cp = tp) && *cp != '\0';) {
				tp = word(cp);
				if (streq(cp, "removeable"))
					v |= D_REMOVABLE;
				else if (streq(cp, "ecc"))
					v |= D_ECC;
				else if (streq(cp, "badsect"))
					v |= D_BADSECT;
				else {
					fprintf(stderr,
					    "line %d: %s: bad flag\n",
					    lineno, cp);
					errors++;
				}
			}
			lp->d_flags = v;
			continue;
		}
		if (streq(cp, "drivedata")) {
			for (i = 0; (cp = tp) && *cp != '\0' && i < NDDATA;) {
				lp->d_drivedata[i++] = strtoul(cp, NULL, 10);
				tp = word(cp);
			}
			continue;
		}
		if (sscanf(cp, "%lu partitions", &v) == 1) {
			if (v == 0 || v > MAXPARTITIONS) {
				fprintf(stderr,
				    "line %d: bad # of partitions\n", lineno);
				lp->d_npartitions = MAXPARTITIONS;
				errors++;
			} else
				lp->d_npartitions = v;
			continue;
		}
		if (tp == NULL)
			tp = blank;
		if (streq(cp, "disk")) {
			strncpy(lp->d_typename, tp, sizeof (lp->d_typename));
			continue;
		}
		if (streq(cp, "label")) {
			strncpy(lp->d_packname, tp, sizeof (lp->d_packname));
			continue;
		}
		if (streq(cp, "bytes/sector")) {
			v = strtoul(tp, NULL, 10);
			if (v == 0 || (v % DEV_BSIZE) != 0) {
				fprintf(stderr,
				    "line %d: %s: bad sector size\n",
				    lineno, tp);
				errors++;
			} else
				lp->d_secsize = v;
			continue;
		}
		if (streq(cp, "sectors/track")) {
			v = strtoul(tp, NULL, 10);
#if (ULONG_MAX != 0xffffffffUL)
			if (v == 0 || v > 0xffffffff) {
#else
			if (v == 0) {
#endif
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_nsectors = v;
			continue;
		}
		if (streq(cp, "sectors/cylinder")) {
			v = strtoul(tp, NULL, 10);
			if (v == 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_secpercyl = v;
			continue;
		}
		if (streq(cp, "tracks/cylinder")) {
			v = strtoul(tp, NULL, 10);
			if (v == 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_ntracks = v;
			continue;
		}
		if (streq(cp, "cylinders")) {
			v = strtoul(tp, NULL, 10);
			if (v == 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_ncylinders = v;
			continue;
		}
		if (streq(cp, "sectors/unit")) {
			v = strtoul(tp, NULL, 10);
			if (v == 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_secperunit = v;
			continue;
		}
		if (streq(cp, "rpm")) {
			v = strtoul(tp, NULL, 10);
			if (v == 0 || v > USHRT_MAX) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_rpm = v;
			continue;
		}
		if (streq(cp, "interleave")) {
			v = strtoul(tp, NULL, 10);
			if (v == 0 || v > USHRT_MAX) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_interleave = v;
			continue;
		}
		if (streq(cp, "trackskew")) {
			v = strtoul(tp, NULL, 10);
			if (v > USHRT_MAX) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_trackskew = v;
			continue;
		}
		if (streq(cp, "cylinderskew")) {
			v = strtoul(tp, NULL, 10);
			if (v > USHRT_MAX) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_cylskew = v;
			continue;
		}
		if (streq(cp, "headswitch")) {
			v = strtoul(tp, NULL, 10);
			lp->d_headswitch = v;
			continue;
		}
		if (streq(cp, "track-to-track seek")) {
			v = strtoul(tp, NULL, 10);
			lp->d_trkseek = v;
			continue;
		}
		/* the ':' was removed above */
		if (*cp < 'a' || *cp > MAX_PART || cp[1] != '\0') {
			fprintf(stderr,
			    "line %d: %s: Unknown disklabel field\n", lineno,
			    cp);
			errors++;
			continue;
		}

		/* Process a partition specification line. */
		part = *cp - 'a';
		if (part >= lp->d_npartitions) {
			fprintf(stderr,
			    "line %d: partition name out of range a-%c: %s\n",
			    lineno, 'a' + lp->d_npartitions - 1, cp);
			errors++;
			continue;
		}
		part_set[part] = 1;

		if (getasciipartspec(tp, lp, part, lineno) != 0) {
			errors++;
			break;
		}
	}
	errors += checklabel(lp);
	return (errors == 0);
}

#define NXTNUM(n) do { \
	if (tp == NULL) { \
		fprintf(stderr, "line %d: too few numeric fields\n", lineno); \
		return (1); \
	} else { \
		cp = tp, tp = word(cp); \
		(n) = strtoul(cp, NULL, 10); \
	} \
} while (0)

/* retain 1 character following number */
#define NXTWORD(w,n) do { \
	if (tp == NULL) { \
		fprintf(stderr, "line %d: too few numeric fields\n", lineno); \
		return (1); \
	} else { \
	        char *tmp; \
		cp = tp, tp = word(cp); \
	        (n) = strtoul(cp, &tmp, 10); \
		if (tmp) (w) = *tmp; \
	} \
} while (0)

/*
 * Read a partition line into partition `part' in the specified disklabel.
 * Return 0 on success, 1 on failure.
 */
int
getasciipartspec(char *tp, struct disklabel *lp, int part, int lineno)
{
	struct partition *pp;
	char *cp;
	const char **cpp;
	u_long v;

	pp = &lp->d_partitions[part];
	cp = NULL;

	v = 0;
	NXTWORD(part_size_type[part],v);
	if (v == 0 && part_size_type[part] != '*') {
		fprintf(stderr,
		    "line %d: %s: bad partition size\n", lineno, cp);
		return (1);
	}
	pp->p_size = v;

	v = 0;
	NXTWORD(part_offset_type[part],v);
	if (v == 0 && part_offset_type[part] != '*' &&
	    part_offset_type[part] != '\0') {
		fprintf(stderr,
		    "line %d: %s: bad partition offset\n", lineno, cp);
		return (1);
	}
	pp->p_offset = v;
	cp = tp, tp = word(cp);
	for (cpp = fstypenames; cpp < &fstypenames[FSMAXTYPES]; cpp++)
		if (*cpp && streq(*cpp, cp))
			break;
	if (*cpp != NULL) {
		pp->p_fstype = cpp - fstypenames;
	} else {
		if (isdigit(*cp))
			v = strtoul(cp, NULL, 10);
		else
			v = FSMAXTYPES;
		if (v >= FSMAXTYPES) {
			fprintf(stderr,
			    "line %d: Warning, unknown file system type %s\n",
			    lineno, cp);
			v = FS_UNUSED;
		}
		pp->p_fstype = v;
	}

	switch (pp->p_fstype) {
	case FS_UNUSED:
		/*
		 * allow us to accept defaults for
		 * fsize/frag/cpg
		 */
		if (tp) {
			NXTNUM(pp->p_fsize);
			if (pp->p_fsize == 0)
				break;
			NXTNUM(v);
			pp->p_frag = v / pp->p_fsize;
		}
		/* else default to 0's */
		break;

	/* These happen to be the same */
	case FS_BSDFFS:
	case FS_BSDLFS:
		if (tp) {
			NXTNUM(pp->p_fsize);
			if (pp->p_fsize == 0)
				break;
			NXTNUM(v);
			pp->p_frag = v / pp->p_fsize;
			NXTNUM(pp->p_cpg);
		} else {
			/*
			 * FIX! poor attempt at adaptive
			 */
			/* 1 GB */
			if (pp->p_size < 1024*1024*1024 / lp->d_secsize) {
				/*
				 * FIX! These are too low, but are traditional
				 */
				pp->p_fsize = DEFAULT_NEWFS_FRAG;
				pp->p_frag = DEFAULT_NEWFS_BLOCK /
				    DEFAULT_NEWFS_FRAG;
				pp->p_cpg = DEFAULT_NEWFS_CPG;
			} else {
				pp->p_fsize = BIG_NEWFS_FRAG;
				pp->p_frag = BIG_NEWFS_BLOCK /
				    BIG_NEWFS_FRAG;
				pp->p_cpg = BIG_NEWFS_CPG;
			}
		}
	default:
		break;
	}
	return (0);
}

/*
 * Check disklabel for errors and fill in
 * derived fields according to supplied values.
 */
int
checklabel(struct disklabel *lp)
{
	struct partition *pp;
	int i, errors = 0;
	char part;
	u_long total_size, total_percent, current_offset;
	int seen_default_offset;
	int hog_part;
	int j;
	struct partition *pp2;

	if (lp->d_secsize == 0) {
		fprintf(stderr, "sector size 0\n");
		return (1);
	}
	if (lp->d_nsectors == 0) {
		fprintf(stderr, "sectors/track 0\n");
		return (1);
	}
	if (lp->d_ntracks == 0) {
		fprintf(stderr, "tracks/cylinder 0\n");
		return (1);
	}
	if  (lp->d_ncylinders == 0) {
		fprintf(stderr, "cylinders/unit 0\n");
		errors++;
	}
	if (lp->d_rpm == 0)
		Warning("revolutions/minute 0");
	if (lp->d_secpercyl == 0)
		lp->d_secpercyl = lp->d_nsectors * lp->d_ntracks;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = lp->d_secpercyl * lp->d_ncylinders;
	if (lp->d_bbsize == 0) {
		fprintf(stderr, "boot block size 0\n");
		errors++;
	} else if (lp->d_bbsize % lp->d_secsize)
		Warning("boot block size %% sector-size != 0");
	if (lp->d_npartitions > MAXPARTITIONS)
		Warning("number of partitions (%lu) > MAXPARTITIONS (%d)",
		    (u_long)lp->d_npartitions, MAXPARTITIONS);

	/* first allocate space to the partitions, then offsets */
	total_size = 0; /* in sectors */
	total_percent = 0; /* in percent */
	hog_part = -1;
	/* find all fixed partitions */
	for (i = 0; i < lp->d_npartitions; i++) {
		pp = &lp->d_partitions[i];
		if (part_set[i]) {
			if (part_size_type[i] == '*') {
				if (i == RAW_PART) {
					pp->p_size = lp->d_secperunit;
				} else {
					if (hog_part != -1)
						Warning("Too many '*' partitions (%c and %c)",
						    hog_part + 'a',i + 'a');
					else
						hog_part = i;
				}
			} else {
				off_t size;

				size = pp->p_size;
				switch (part_size_type[i]) {
				case '%':
					total_percent += size;
					break;
				case 'k':
				case 'K':
					size *= 1024ULL;
					break;
				case 'm':
				case 'M':
					size *= 1024ULL * 1024ULL;
					break;
				case 'g':
				case 'G':
					size *= 1024ULL * 1024ULL * 1024ULL;
					break;
				case '\0':
					break;
				default:
					Warning("unknown size specifier '%c' (K/M/G are valid)",part_size_type[i]);
					break;
				}
				/* don't count %'s yet */
				if (part_size_type[i] != '%') {
					/*
					 * for all not in sectors, convert to
					 * sectors
					 */
					if (part_size_type[i] != '\0') {
						if (size % lp->d_secsize != 0)
							Warning("partition %c not an integer number of sectors",
							    i + 'a');
						size /= lp->d_secsize;
						pp->p_size = size;
					}
					/* else already in sectors */
					if (i != RAW_PART)
						total_size += size;
				}
			}
		}
	}
	/* handle % partitions - note %'s don't need to add up to 100! */
	if (total_percent != 0) {
		long free_space = lp->d_secperunit - total_size;
		if (total_percent > 100) {
			fprintf(stderr,"total percentage %lu is greater than 100\n",
			    total_percent);
			errors++;
		}

		if (free_space > 0) {
			for (i = 0; i < lp->d_npartitions; i++) {
				pp = &lp->d_partitions[i];
				if (part_set[i] && part_size_type[i] == '%') {
					/* careful of overflows! and integer roundoff */
					pp->p_size = ((double)pp->p_size/100) * free_space;
					total_size += pp->p_size;

					/* FIX we can lose a sector or so due to roundoff per
					   partition.  A more complex algorithm could avoid that */
				}
			}
		} else {
			fprintf(stderr,
			    "%ld sectors available to give to '*' and '%%' partitions\n",
			    free_space);
			errors++;
			/* fix?  set all % partitions to size 0? */
		}
	}
	/* give anything remaining to the hog partition */
	if (hog_part != -1) {
		lp->d_partitions[hog_part].p_size = lp->d_secperunit - total_size;
		total_size = lp->d_secperunit;
	}

	/* Now set the offsets for each partition */
	current_offset = 0; /* in sectors */
	seen_default_offset = 0;
	for (i = 0; i < lp->d_npartitions; i++) {
		part = 'a' + i;
		pp = &lp->d_partitions[i];
		if (part_set[i]) {
			if (part_offset_type[i] == '*') {
				if (i == RAW_PART) {
					pp->p_offset = 0;
				} else {
					pp->p_offset = current_offset;
					seen_default_offset = 1;
				}
			} else {
				/* allow them to be out of order for old-style tables */
				if (pp->p_offset < current_offset && 
				    seen_default_offset && i != RAW_PART &&
				    pp->p_fstype != FS_VINUM) {
					fprintf(stderr,
"Offset %ld for partition %c overlaps previous partition which ends at %lu\n",
					    (long)pp->p_offset,i+'a',current_offset);
					fprintf(stderr,
"Labels with any *'s for offset must be in ascending order by sector\n");
					errors++;
				} else if (pp->p_offset != current_offset &&
				    i != RAW_PART && seen_default_offset) {
					/* 
					 * this may give unneeded warnings if 
					 * partitions are out-of-order
					 */
					Warning(
"Offset %ld for partition %c doesn't match expected value %ld",
					    (long)pp->p_offset, i + 'a', current_offset);
				}
			}
			if (i != RAW_PART)
				current_offset = pp->p_offset + pp->p_size; 
		}
	}

	for (i = 0; i < lp->d_npartitions; i++) {
		part = 'a' + i;
		pp = &lp->d_partitions[i];
		if (pp->p_size == 0 && pp->p_offset != 0)
			Warning("partition %c: size 0, but offset %lu",
			    part, (u_long)pp->p_offset);
#ifdef __sparc64__
		/* See comment in writelabel(). */
		if (pp->p_offset % lp->d_secpercyl != 0) {
			fprintf(stderr, "partition %c: does not start on a "
			    "cylinder boundary!\n", part);
			errors++;
		}
#endif
#ifdef notdef
		if (pp->p_size % lp->d_secpercyl)
			Warning("partition %c: size %% cylinder-size != 0",
			    part);
		if (pp->p_offset % lp->d_secpercyl)
			Warning("partition %c: offset %% cylinder-size != 0",
			    part);
#endif
		if (pp->p_offset > lp->d_secperunit) {
			fprintf(stderr,
			    "partition %c: offset past end of unit\n", part);
			errors++;
		}
		if (pp->p_offset + pp->p_size > lp->d_secperunit) {
			fprintf(stderr,
			"partition %c: partition extends past end of unit\n",
			    part);
			errors++;
		}
		if (i == RAW_PART)
		{
			if (pp->p_fstype != FS_UNUSED)
				Warning("partition %c is not marked as unused!",part);
			if (pp->p_offset != 0)
				Warning("partition %c doesn't start at 0!",part);
			if (pp->p_size != lp->d_secperunit)
				Warning("partition %c doesn't cover the whole unit!",part);

			if ((pp->p_fstype != FS_UNUSED) || (pp->p_offset != 0) ||
			    (pp->p_size != lp->d_secperunit)) {
				Warning("An incorrect partition %c may cause problems for "
				    "standard system utilities",part);
			}
		}

		/* check for overlaps */
		/* this will check for all possible overlaps once and only once */
		for (j = 0; j < i; j++) {
			pp2 = &lp->d_partitions[j];
			if (j != RAW_PART && i != RAW_PART &&	
			    pp->p_fstype != FS_VINUM &&
			    pp2->p_fstype != FS_VINUM &&
			    part_set[i] && part_set[j]) {
				if (pp2->p_offset < pp->p_offset + pp->p_size &&
				    (pp2->p_offset + pp2->p_size > pp->p_offset ||
					pp2->p_offset >= pp->p_offset)) {
					fprintf(stderr,"partitions %c and %c overlap!\n",
					    j + 'a', i + 'a');
					errors++;
				}
			}
		}
	}
	for (; i < MAXPARTITIONS; i++) {
		part = 'a' + i;
		pp = &lp->d_partitions[i];
		if (pp->p_size || pp->p_offset)
			Warning("unused partition %c: size %d offset %lu",
			    'a' + i, pp->p_size, (u_long)pp->p_offset);
	}
	return (errors);
}

/*
 * When operating on a "virgin" disk, try getting an initial label
 * from the associated device driver.  This might work for all device
 * drivers that are able to fetch some initial device parameters
 * without even having access to a (BSD) disklabel, like SCSI disks,
 * most IDE drives, or vn devices.
 *
 * The device name must be given in its "canonical" form.
 */
struct disklabel *
getvirginlabel(void)
{
	static struct disklabel loclab;
	struct partition *dp;
	char lnamebuf[BBSIZE];
	int f;
	u_int secsize, u;
	off_t mediasize;

	if (dkname[0] == '/') {
		warnx("\"auto\" requires the usage of a canonical disk name");
		return (NULL);
	}
	(void)snprintf(lnamebuf, BBSIZE, "%s%s", _PATH_DEV, dkname);
	if ((f = open(lnamebuf, O_RDONLY)) == -1) {
		warn("cannot open %s", lnamebuf);
		return (NULL);
	}

	/* New world order */
	if ((ioctl(f, DIOCGMEDIASIZE, &mediasize) != 0) ||
	    (ioctl(f, DIOCGSECTORSIZE, &secsize) != 0)) {
		close (f);
		return (NULL);
	}
	memset(&loclab, 0, sizeof loclab);
	loclab.d_magic = DISKMAGIC;
	loclab.d_magic2 = DISKMAGIC;
	loclab.d_secsize = secsize;
	loclab.d_secperunit = mediasize / secsize;

	/*
	 * Nobody in these enligthened days uses the CHS geometry for
	 * anything, but nontheless try to get it right.  If we fail
	 * to get any good ideas from the device, construct something
	 * which is IBM-PC friendly.
	 */
	if (ioctl(f, DIOCGFWSECTORS, &u) == 0)
		loclab.d_nsectors = u;
	else
		loclab.d_nsectors = 63;
	if (ioctl(f, DIOCGFWHEADS, &u) == 0)
		loclab.d_ntracks = u;
	else if (loclab.d_secperunit <= 63*1*1024)
		loclab.d_ntracks = 1;
	else if (loclab.d_secperunit <= 63*16*1024)
		loclab.d_ntracks = 16;
	else
		loclab.d_ntracks = 255;
	loclab.d_secpercyl = loclab.d_ntracks * loclab.d_nsectors;
	loclab.d_ncylinders = loclab.d_secperunit / loclab.d_secpercyl;
	loclab.d_npartitions = MAXPARTITIONS;

	/* Various (unneeded) compat stuff */
	loclab.d_rpm = 3600;
	loclab.d_bbsize = BBSIZE;
	loclab.d_interleave = 1;;
	strncpy(loclab.d_typename, "amnesiac",
	    sizeof(loclab.d_typename));

	dp = &loclab.d_partitions[RAW_PART];
	dp->p_size = loclab.d_secperunit;
	loclab.d_checksum = dkcksum(&loclab);
	close (f);
	return (&loclab);
}

/*
 * If we are installing a boot program that doesn't fit in d_bbsize
 * we need to mark those partitions that the boot overflows into.
 * This allows newfs to prevent creation of a file system where it might
 * clobber bootstrap code.
 */
void
setbootflag(struct disklabel *lp)
{
	struct partition *pp;
	int i, errors = 0;
	char part;
	u_long boffset;

	if (bootbuf == 0)
		return;
	boffset = bootsize / lp->d_secsize;
	for (i = 0; i < lp->d_npartitions; i++) {
		part = 'a' + i;
		pp = &lp->d_partitions[i];
		if (pp->p_size == 0)
			continue;
		if (boffset <= pp->p_offset) {
			if (pp->p_fstype == FS_BOOT)
				pp->p_fstype = FS_UNUSED;
		} else if (pp->p_fstype != FS_BOOT) {
			if (pp->p_fstype != FS_UNUSED) {
				fprintf(stderr,
					"boot overlaps used partition %c\n",
					part);
				errors++;
			} else {
				pp->p_fstype = FS_BOOT;
				Warning("boot overlaps partition %c, %s",
					part, "marked as FS_BOOT");
			}
		}
	}
	if (errors)
		errx(4, "cannot install boot program");
}

/*VARARGS1*/
void
Warning(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "Warning, ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

void
usage(void)
{
#if NUMBOOT > 0
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		"usage: disklabel [-r] disk",
		"\t\t(to read label)",
		"       disklabel -w [-r] [-n] disk type [ packid ]",
		"\t\t(to write label with existing boot program)",
		"       disklabel -e [-r] [-n] disk",
		"\t\t(to edit label)",
		"       disklabel -R [-r] [-n] disk protofile",
		"\t\t(to restore label with existing boot program)",
#if NUMBOOT > 1
		"       disklabel -B [-n] [ -b boot1 [ -s boot2 ] ] disk [ type ]",
		"\t\t(to install boot program with existing label)",
		"       disklabel -w -B [-n] [ -b boot1 [ -s boot2 ] ] disk type [ packid ]",
		"\t\t(to write label and boot program)",
		"       disklabel -R -B [-n] [ -b boot1 [ -s boot2 ] ] disk protofile [ type ]",
		"\t\t(to restore label and boot program)",
#else
		"       disklabel -B [-n] [ -b bootprog ] disk [ type ]",
		"\t\t(to install boot program with existing on-disk label)",
		"       disklabel -w -B [-n] [ -b bootprog ] disk type [ packid ]",
		"\t\t(to write label and install boot program)",
		"       disklabel -R -B [-n] [ -b bootprog ] disk protofile [ type ]",
		"\t\t(to restore label and install boot program)",
#endif
		"       disklabel [-NW] disk",
		"\t\t(to write disable/enable label)");
#else
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		"usage: disklabel [-r] disk", "(to read label)",
		"       disklabel -w [-r] [-n] disk type [ packid ]",
		"\t\t(to write label)",
		"       disklabel -e [-r] [-n] disk",
		"\t\t(to edit label)",
		"       disklabel -R [-r] [-n] disk protofile",
		"\t\t(to restore label)",
		"       disklabel [-NW] disk",
		"\t\t(to write disable/enable label)");
#endif
	exit(1);
}
