/*
 * Copyright (c) 1987 The Regents of the University of California.
 * All rights reserved.
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
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00034
 * --------------------         -----   ----------------------
 *
 * 18 Sep 92	Gary A Browning		Fix expectation of active partition
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1987 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)disklabel.c	5.20 (Berkeley) 2/9/91";
/* from static char sccsid[] = "@(#)disklabel.c	1.2 (Symmetric) 11/28/85"; */
#endif /* not lint */

#include <sys/param.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <ufs/fs.h>
#include <string.h>
#define DKTYPENAMES
#include <sys/disklabel.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "pathnames.h"


/*
 * Disklabel: read and write disklabels.
 * The label is usually placed on one of the first sectors of the disk.
 * Many machines (VAX 11/750) also place a bootstrap in the same area,
 * in which case the label is embedded in the bootstrap.
 * The bootstrap source must leave space at the proper offset
 * for the label on such machines.
 *
 * On 386BSD, the disklabel may either be at the start of the disk, or, at
 * the start of an MS/DOS partition. In this way, it can be used either
 * in concert with other operating systems sharing a disk, or with the
 * disk dedicated to 386BSD. In shared mode, the DOS disk geometry must be
 * identical to that which disklabel uses, and the disklabel must solely
 * describe the space within the partition selected. Otherwise, the disk
 * must be dedicated to 386BSD. -wfj
 */

#if defined(vax)
#define RAWPARTITION	'c'
#endif

#if defined(__386BSD__)
/* with 386BSD, 'c' maps the portion of the disk given over to 386BSD,
   and 'd' maps the entire drive, ignoring any partition tables */
#define LABELPARTITION	('c' - 'a')
#define RAWPARTITION	'd'
#endif

#ifndef RAWPARTITION
#define RAWPARTITION	'a'
#endif

#ifndef BBSIZE
#define	BBSIZE	8192			/* size of boot area, with label */
#endif

#if defined(vax) || defined(__386BSD__)
#define	BOOT				/* also have bootstrap in "boot area" */
#define	BOOTDIR	_PATH_BOOTDIR		/* source of boot binaries */
#else
#ifdef lint
#define	BOOT
#endif
#endif

#define	DEFEDITOR	_PATH_VI
#define	streq(a,b)	(strcmp(a,b) == 0)

#ifdef BOOT
char	*xxboot;
char	*bootxx;
#endif

char	*dkname;
char	*specname;
char	tmpfil[] = _PATH_TMP;

extern	int errno;
char	namebuf[BBSIZE], *np = namebuf;
struct	disklabel lab;
struct	disklabel *readlabel(), *makebootarea();
char	bootarea[BBSIZE];
char	boot0[MAXPATHLEN];
char	boot1[MAXPATHLEN];

enum	{ UNSPEC, EDIT, NOWRITE, READ, RESTORE, WRITE, WRITEABLE } op = UNSPEC;

int	rflag;

#ifdef DEBUG
int	debug;
#endif

#ifdef __386BSD__
struct dos_partition *dosdp;	/* 386BSD DOS partition, if found */
struct dos_partition *readmbr(int);
#endif

main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	register struct disklabel *lp;
	FILE *t;
	int ch, f, error = 0;
	char *name = 0, *type;

	while ((ch = getopt(argc, argv, "NRWerw")) != EOF)
		switch (ch) {
			case 'N':
				if (op != UNSPEC)
					usage();
				op = NOWRITE;
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
#ifdef DEBUG
			case 'd':
				debug++;
				break;
#endif
			case '?':
			default:
				usage();
		}
	argc -= optind;
	argv += optind;
	if (op == UNSPEC)
		op = READ;
	if (argc < 1)
		usage();

	dkname = argv[0];
	if (dkname[0] != '/') {
		(void)sprintf(np, "%sr%s%c", _PATH_DEV, dkname, RAWPARTITION);
		specname = np;
		np += strlen(specname) + 1;
	} else
		specname = dkname;
	f = open(specname, op == READ ? O_RDONLY : O_RDWR);
	if (f < 0 && errno == ENOENT && dkname[0] != '/') {
		(void)sprintf(specname, "%sr%s", _PATH_DEV, dkname);
		np = namebuf + strlen(specname) + 1;
		f = open(specname, op == READ ? O_RDONLY : O_RDWR);
	}
	if (f < 0)
		Perror(specname);

#ifdef	__386BSD__
	/*
	 * Check for presence of DOS partition table in
	 * master boot record. Return pointer to 386BSD
	 * partition, if present. If no valid partition table,
	 * return 0. If valid partition table present, but no
	 * partition to use, return a pointer to a non-386bsd
	 * partition.
	 */
	dosdp = readmbr(f);
#ifdef notdef /* not used (some bootstraps copy wd tables to 0x300) */
	{ int mfd; unsigned char params[0x10];
		/* sleezy, but we need it fast! */
		mfd = open("/dev/mem", 0);
		lseek(mfd, 0x300, 0);
		read (mfd, params, 0x10);
	}
#endif

#endif

	switch(op) {
	case EDIT:
		if (argc != 1)
			usage();
		lp = readlabel(f);
		if (lp == NULL) {
			/*
			 * It's too much trouble to make -e -r work
			 * when there is no on-disk label.
			 *
			 * XXX -e without -r will fail if there is no
			 * on-disk label, but not until the user has
			 * wasted time editing the in-core label.
			 */
			errno = ESRCH;
			l_perror("-e flag is not suitable");
			exit(1);
		}
		error = edit(lp, f);
		break;
	case NOWRITE: {
		int flag = 0;
		if (ioctl(f, DIOCWLABEL, (char *)&flag) < 0)
			Perror("ioctl DIOCWLABEL");
		break;
	}
	case READ:
		if (argc != 1)
			usage();
			
		lp = readlabel(f);
		if (lp == NULL) {
			fprintf(stderr,
			"Ignoring -r and trying to read in-core label\n");
			rflag = 0;
			lp = readlabel(f);
		}
		display(stdout, lp);
		error = checklabel(lp);
		break;
	case RESTORE:
#ifdef BOOT
		if (rflag) {
			if (argc == 4) {	/* [ priboot secboot ] */
				xxboot = argv[2];
				bootxx = argv[3];
				lab.d_secsize = DEV_BSIZE;	/* XXX */
				lab.d_bbsize = BBSIZE;		/* XXX */
			}
			else if (argc == 3) 	/* [ disktype ] */
				makelabel(argv[2], (char *)NULL, &lab);
			else {
				fprintf(stderr,
"Must specify either disktype or bootfiles with -r flag of RESTORE option\n");
				exit(1);
			}
		}
		else
#endif
		if (argc != 2)
			usage();
		lp = makebootarea(bootarea, &lab);
		if (!(t = fopen(argv[1],"r")))
			Perror(argv[1]);
		if (getasciilabel(t, lp))
			error = writelabel(f, bootarea, lp);
		break;
	case WRITE:
		type = argv[1];
#ifdef BOOT
		if (argc > 5 || argc < 2)
			usage();
		if (argc > 3) {
			bootxx = argv[--argc];
			xxboot = argv[--argc];
		}
#else
		if (argc > 3 || argc < 2)
			usage();
#endif
		if (argc > 2)
			name = argv[--argc];
		makelabel(type, name, &lab);
		lp = makebootarea(bootarea, &lab);
		*lp = lab;
		if (checklabel(lp) == 0)
			error = writelabel(f, bootarea, lp);
		break;
	case WRITEABLE: {
		int flag = 1;
		if (ioctl(f, DIOCWLABEL, (char *)&flag) < 0)
			Perror("ioctl DIOCWLABEL");
		break;
	}
	}
	exit(error);
}

/*
 * Construct a prototype disklabel from /etc/disktab.  As a side
 * effect, set the names of the primary and secondary boot files
 * if specified.
 */
makelabel(type, name, lp)
	char *type, *name;
	register struct disklabel *lp;
{
	register struct disklabel *dp;
	char *strcpy();

	dp = getdiskbyname(type);
	if (dp == NULL) {
		fprintf(stderr, "%s: unknown disk type\n", type);
		exit(1);
	}
	*lp = *dp;
#ifdef BOOT
	/*
	 * Check if disktab specifies the bootstraps (b0 or b1).
	 */
	if (!xxboot && lp->d_boot0) {
		if (*lp->d_boot0 != '/')
			(void)sprintf(boot0, "%s/%s", BOOTDIR, lp->d_boot0);
		else
			(void)strcpy(boot0, lp->d_boot0);
		xxboot = boot0;
	}
	if (!bootxx && lp->d_boot1) {
		if (*lp->d_boot1 != '/')
			(void)sprintf(boot1, "%s/%s", BOOTDIR, lp->d_boot1);
		else
			(void)strcpy(boot1, lp->d_boot1);
		bootxx = boot1;
	}
	/*
	 * If bootstraps not specified anywhere, makebootarea()
	 * will choose ones based on the name of the disk special
	 * file. E.g. /dev/ra0 -> raboot, bootra
	 */
#endif /*BOOT*/
	/* d_packname is union d_boot[01], so zero */
	bzero(lp->d_packname, sizeof(lp->d_packname));
	if (name)
		(void)strncpy(lp->d_packname, name, sizeof(lp->d_packname));
}

writelabel(f, boot, lp)
	int f;
	char *boot;
	register struct disklabel *lp;
{
	register int i;
	int flag;
#ifdef	__386BSD__
	off_t lbl_off; struct partition *pp = lp->d_partitions + LABELPARTITION;
#endif

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);
	if (rflag) {

#ifdef	__386BSD__
		/*
		 * If 386BSD DOS partition is missing, or if 
		 * the label to be written is not within partition,
		 * prompt first. Need to allow this in case operator
		 * wants to convert the drive for dedicated use.
		 * In this case, partition 'c' had better start at 0,
		 * otherwise we reject the request as meaningless. -wfj
		 */

		if (dosdp && dosdp->dp_typ == DOSPTYP_386BSD && pp->p_size &&
			dosdp->dp_start == pp->p_offset) {
			lbl_off = pp->p_offset;
		} else {
			if (dosdp) {
				int c;

				printf("overwrite DOS partition table? [n]: ");
				fflush(stdout);
				c = getchar();
				if (c != EOF && c != '\n')
					while (getchar() != '\n')
						;
				if  (c != 'y')
					exit(0);
			}
			lbl_off = 0;
		}
		(void)lseek(f, (off_t)(lbl_off * lp->d_secsize), L_SET);
#endif
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
			/*return (1);*/
		}
		
		/*
		 * write enable label sector before write (if necessary),
		 * disable after writing.
		 */
		flag = 1;
		if (ioctl(f, DIOCWLABEL, &flag) < 0)
			perror("ioctl DIOCWLABEL");
		if (write(f, boot, lp->d_bbsize) != lp->d_bbsize) {
			perror("write");
			return (1);
		}
		flag = 0;
		(void) ioctl(f, DIOCWLABEL, &flag);
	} else if (ioctl(f, DIOCWDINFO, lp) < 0) {
		l_perror("ioctl DIOCWDINFO");
		return (1);
	}

	if (lp->d_type != DTYPE_SCSI && lp->d_flags & D_BADSECT) {
		daddr_t alt;

		/*
		 * XXX this knows too much about bad144 internals
		 */
#ifdef __386BSD__
#define BAD144_PART	2	/* XXX scattered magic numbers */
#define BSD_PART	0	/* XXX should be 2 but bad144.c uses 0 */
		if (lp->d_partitions[BSD_PART].p_offset != 0)
			alt = lp->d_partitions[BAD144_PART].p_offset
			      + lp->d_partitions[BAD144_PART].p_size;
		else
#endif
			alt = lp->d_secperunit;
		alt -= lp->d_nsectors;
		for (i = 1; i < 11 && i < lp->d_nsectors; i += 2) {
			lseek(f, (off_t)((alt + i) * lp->d_secsize), L_SET);
			if (write(f, boot + (LABELSECTOR * lp->d_secsize),
				  lp->d_secsize) < lp->d_secsize) {
				int oerrno = errno;
				fprintf(stderr, "alternate label %d ", i/2);
				errno = oerrno;
				perror("write");
			}
		}
	}

	return (0);
}

l_perror(s)
	char *s;
{
	int saverrno = errno;

	fprintf(stderr, "disklabel: %s: ", s);

	switch (saverrno) {

	case ESRCH:
		fprintf(stderr, "No disk label on disk;\n");
		fprintf(stderr,
	"use flags \"-w -r\" or \"-R -r\" to install initial label\n");
		break;

	case EINVAL:
		fprintf(stderr, "Label magic number or checksum is wrong!\n");
		fprintf(stderr, "(disklabel or kernel is out of date?)\n");
		break;

	case EBUSY:
		fprintf(stderr, "Open partition would move or shrink\n");
		break;

	case EXDEV:
		fprintf(stderr,
	"Labeled partition or 'a' partition must start at beginning of disk\n");
		fprintf(stderr, "or DOS partition\n");
		break;

	default:
		errno = saverrno;
		perror((char *)NULL);
		break;
	}
}

#ifdef __386BSD__
/*
 * Fetch DOS partition table from disk.
 */
struct dos_partition *
readmbr(f)
	int f;
{
	static struct dos_partition dos_partitions[NDOSPART];
	struct dos_partition *dp, *bsdp;
	char mbr[DEV_BSIZE];	/* XXX - DOS_DEV_BSIZE */
	int i, npart, nboot, njunk;

	(void)lseek(f, (off_t)DOSBBSECTOR, L_SET);
	if (read(f, mbr, sizeof(mbr)) < sizeof(mbr))
		Perror("can't read master boot record");
		
	bcopy(mbr + DOSPARTOFF, dos_partitions, sizeof(dos_partitions));

	/*
	 * Don't (yet) know disk geometry (BIOS), use
	 * partition table to find 386BSD partition, and obtain
	 * disklabel from there.
	 *
	 * XXX - the checks for a valid partition table are inadequate.
	 * This gets called for floppies, which will never have an mbr
	 * and may have junk that looks like a partition table...
	 */
	dp = dos_partitions;
	npart = njunk = nboot = 0;
	bsdp = NULL;
	for (i = 0; i < NDOSPART; i++, dp++) {
		if (dp->dp_flag != 0x80 && dp->dp_flag != 0) njunk++;
		else
			if (dp->dp_size > 0) npart++;
		if (dp->dp_flag == 0x80) nboot++;
		if (dp->dp_size && dp->dp_typ == DOSPTYP_386BSD)
			bsdp = dp;
	}

	/* valid partition table? */
	/*
	 * XXX - ignore `nboot'.  Drives other than the first do not need a
	 * boot flag.  The first drive doesn't need a boot flag when it is
	 * booted from a multi-boot program.
	 */
	if (npart == 0 || njunk)			/* 18 Sep 92*/
/* was:	if (nboot != 1 || npart == 0 || njunk)*/
		return (0);
	/* if no bsd partition, pass back first one */
	if (!bsdp) {
		Warning("DOS partition table with no valid 386BSD partition");
		return (dos_partitions);
	}
	return (bsdp);
}
#endif

/*
 * Fetch disklabel for disk.
 * Use ioctl to get label unless -r flag is given.
 */
struct disklabel *
readlabel(f)
	int f;
{
	register struct disklabel *lp;

	if (rflag) {
#ifdef __386BSD__
		off_t sectoffset;

		if (dosdp && dosdp->dp_size && dosdp->dp_typ == DOSPTYP_386BSD)
			sectoffset = dosdp->dp_start * DEV_BSIZE;
		else
			sectoffset = 0;
		(void)lseek(f, sectoffset, L_SET);
#endif
			
		if (read(f, bootarea, BBSIZE) < BBSIZE)
			Perror(specname);
		for (lp = (struct disklabel *)bootarea;
		    lp <= (struct disklabel *)(bootarea + BBSIZE - sizeof(*lp));
		    lp = (struct disklabel *)((char *)lp + 16))
			if (lp->d_magic == DISKMAGIC &&
			    lp->d_magic2 == DISKMAGIC)
				break;
		if (lp > (struct disklabel *)(bootarea+BBSIZE-sizeof(*lp)) ||
		    lp->d_magic != DISKMAGIC || lp->d_magic2 != DISKMAGIC ||
		    dkcksum(lp) != 0) {
			fprintf(stderr,
	"Bad pack magic number (label is damaged, or pack is unlabeled)\n");
			return (NULL);
		}
	} else {
		lp = &lab;
		if (ioctl(f, DIOCGDINFO, lp) < 0)
			Perror("ioctl DIOCGDINFO");
	}
	return (lp);
}

struct disklabel *
makebootarea(boot, dp)
	char *boot;
	register struct disklabel *dp;
{
	struct disklabel *lp;
	register char *p;
	int b;
#ifdef BOOT
	char	*dkbasename;
#endif /*BOOT*/

	lp = (struct disklabel *)(boot + (LABELSECTOR * dp->d_secsize) +
	    LABELOFFSET);
#ifdef BOOT
	if (!rflag)
		return (lp);

	if (xxboot == NULL || bootxx == NULL) {
		dkbasename = np;
		if ((p = rindex(dkname, '/')) == NULL)
			p = dkname;
		else
			p++;
		while (*p && !isdigit(*p))
			*np++ = *p++;
		*np++ = '\0';

		if (xxboot == NULL) {
			(void)sprintf(np, "%s/%sboot", BOOTDIR, dkbasename);
			if (access(np, F_OK) < 0 && dkbasename[0] == 'r')
				dkbasename++;
			xxboot = np;
			(void)sprintf(xxboot, "%s/%sboot", BOOTDIR, dkbasename);
			np += strlen(xxboot) + 1;
		}
		if (bootxx == NULL) {
			(void)sprintf(np, "%s/boot%s", BOOTDIR, dkbasename);
			if (access(np, F_OK) < 0 && dkbasename[0] == 'r')
				dkbasename++;
			bootxx = np;
			(void)sprintf(bootxx, "%s/boot%s", BOOTDIR, dkbasename);
			np += strlen(bootxx) + 1;
		}
	}
#ifdef DEBUG
	if (debug)
		fprintf(stderr, "bootstraps: xxboot = %s, bootxx = %s\n",
			xxboot, bootxx);
#endif

	b = open(xxboot, O_RDONLY);
	if (b < 0)
		Perror(xxboot);
	if (read(b, boot, (int)dp->d_secsize) < 0)
		Perror(xxboot);
	close(b);
	b = open(bootxx, O_RDONLY);
	if (b < 0)
		Perror(bootxx);
	if (read(b, &boot[dp->d_secsize], (int)(dp->d_bbsize-dp->d_secsize)) < 0)
		Perror(bootxx);
	(void)close(b);
#endif /*BOOT*/

	for (p = (char *)lp; p < (char *)lp + sizeof(struct disklabel); p++)
		if (*p) {
			fprintf(stderr,
			    "Bootstrap doesn't leave room for disk label\n");
			exit(2);
		}
	return (lp);
}

display(f, lp)
	FILE *f;
	register struct disklabel *lp;
{
	register int i, j;
	register struct partition *pp;

	fprintf(f, "# %s:\n", specname);
	if ((unsigned) lp->d_type < DKMAXTYPES)
		fprintf(f, "type: %s\n", dktypenames[lp->d_type]);
	else
		fprintf(f, "type: %d\n", lp->d_type);
	fprintf(f, "disk: %.*s\n", sizeof(lp->d_typename), lp->d_typename);
	fprintf(f, "label: %.*s\n", sizeof(lp->d_packname), lp->d_packname);
	fprintf(f, "flags:");
	if (lp->d_flags & D_REMOVABLE)
		fprintf(f, " removeable");
	if (lp->d_flags & D_ECC)
		fprintf(f, " ecc");
	if (lp->d_flags & D_BADSECT)
		fprintf(f, " badsect");
	fprintf(f, "\n");
	fprintf(f, "bytes/sector: %d\n", lp->d_secsize);
	fprintf(f, "sectors/track: %d\n", lp->d_nsectors);
	fprintf(f, "tracks/cylinder: %d\n", lp->d_ntracks);
	fprintf(f, "sectors/cylinder: %d\n", lp->d_secpercyl);
	fprintf(f, "cylinders: %d\n", lp->d_ncylinders);
	fprintf(f, "rpm: %d\n", lp->d_rpm);
	fprintf(f, "interleave: %d\n", lp->d_interleave);
	fprintf(f, "trackskew: %d\n", lp->d_trackskew);
	fprintf(f, "cylinderskew: %d\n", lp->d_cylskew);
	fprintf(f, "headswitch: %d\t\t# milliseconds\n", lp->d_headswitch);
	fprintf(f, "track-to-track seek: %d\t# milliseconds\n", lp->d_trkseek);
	fprintf(f, "drivedata: ");
	for (i = NDDATA - 1; i >= 0; i--)
		if (lp->d_drivedata[i])
			break;
	if (i < 0)
		i = 0;
	for (j = 0; j <= i; j++)
		fprintf(f, "%d ", lp->d_drivedata[j]);
	fprintf(f, "\n\n%d partitions:\n", lp->d_npartitions);
	fprintf(f,
	    "#        size   offset    fstype   [fsize bsize   cpg]\n");
	pp = lp->d_partitions;
	for (i = 0; i < lp->d_npartitions; i++, pp++) {
		if (pp->p_size) {
			fprintf(f, "  %c: %8d %8d  ", 'a' + i,
			   pp->p_size, pp->p_offset);
			if ((unsigned) pp->p_fstype < FSMAXTYPES)
				fprintf(f, "%8.8s", fstypenames[pp->p_fstype]);
			else
				fprintf(f, "%8d", pp->p_fstype);
			switch (pp->p_fstype) {

			case FS_UNUSED:				/* XXX */
				fprintf(f, "    %5d %5d %5.5s ",
				    pp->p_fsize, pp->p_fsize * pp->p_frag, "");
				break;

			case FS_BSDFFS:
				fprintf(f, "    %5d %5d %5d ",
				    pp->p_fsize, pp->p_fsize * pp->p_frag,
				    pp->p_cpg);
				break;

			default:
				fprintf(f, "%20.20s", "");
				break;
			}
			fprintf(f, "\t# (Cyl. %4d",
			    pp->p_offset / lp->d_secpercyl);
			if (pp->p_offset % lp->d_secpercyl)
			    putc('*', f);
			else
			    putc(' ', f);
			fprintf(f, "- %d",
			    (pp->p_offset + 
			    pp->p_size + lp->d_secpercyl - 1) /
			    lp->d_secpercyl - 1);
			if ((pp->p_offset + pp->p_size) % lp->d_secpercyl)
			    putc('*', f);
			fprintf(f, ")\n");
		}
	}
	fflush(f);
}

edit(lp, f)
	struct disklabel *lp;
	int f;
{
	register int c;
	struct disklabel label;
	FILE *fd;
	char *mktemp();

	(void) mktemp(tmpfil);
	fd = fopen(tmpfil, "w");
	if (fd == NULL) {
		fprintf(stderr, "%s: Can't create\n", tmpfil);
		return (1);
	}
	(void)fchmod(fd, 0600);
	display(fd, lp);
	fclose(fd);
	for (;;) {
		if (!editit())
			break;
		fd = fopen(tmpfil, "r");
		if (fd == NULL) {
			fprintf(stderr, "%s: Can't reopen for reading\n",
				tmpfil);
			break;
		}
		bzero((char *)&label, sizeof(label));
		if (getasciilabel(fd, &label)) {
			*lp = label;
			if (writelabel(f, bootarea, lp) == 0) {
				(void) unlink(tmpfil);
				return (0);
			}
		}
		printf("re-edit the label? [y]: "); fflush(stdout);
		c = getchar();
		if (c != EOF && c != '\n')
			while (getchar() != '\n')
				;
		if  (c == 'n')
			break;
	}
	(void) unlink(tmpfil);
	return (1);
}

editit()
{
	register int pid, xpid;
	int stat, omask;
	extern char *getenv();

	omask = sigblock(sigmask(SIGINT)|sigmask(SIGQUIT)|sigmask(SIGHUP));
	while ((pid = fork()) < 0) {
		extern int errno;

		if (errno == EPROCLIM) {
			fprintf(stderr, "You have too many processes\n");
			return(0);
		}
		if (errno != EAGAIN) {
			perror("fork");
			return(0);
		}
		sleep(1);
	}
	if (pid == 0) {
		register char *ed;

		sigsetmask(omask);
		setgid(getgid());
		setuid(getuid());
		if ((ed = getenv("EDITOR")) == (char *)0)
			ed = DEFEDITOR;
		execlp(ed, ed, tmpfil, 0);
		perror(ed);
		exit(1);
	}
	while ((xpid = wait(&stat)) >= 0)
		if (xpid == pid)
			break;
	sigsetmask(omask);
	return(!stat);
}

char *
skip(cp)
	register char *cp;
{

	while (*cp != '\0' && isspace(*cp))
		cp++;
	if (*cp == '\0' || *cp == '#')
		return ((char *)NULL);
	return (cp);
}

char *
word(cp)
	register char *cp;
{
	register char c;

	while (*cp != '\0' && !isspace(*cp) && *cp != '#')
		cp++;
	if ((c = *cp) != '\0') {
		*cp++ = '\0';
		if (c != '#')
			return (skip(cp));
	}
	return ((char *)NULL);
}

/*
 * Read an ascii label in from fd f,
 * in the same format as that put out by display(),
 * and fill in lp.
 */
getasciilabel(f, lp)
	FILE	*f;
	register struct disklabel *lp;
{
	register char **cpp, *cp;
	register struct partition *pp;
	char *tp, *s, line[BUFSIZ];
	int v, lineno = 0, errors = 0;

	lp->d_bbsize = BBSIZE;				/* XXX */
	lp->d_sbsize = SBSIZE;				/* XXX */
	while (fgets(line, sizeof(line) - 1, f)) {
		lineno++;
		if (cp = index(line,'\n'))
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
				tp = "unknown";
			cpp = dktypenames;
			for (; cpp < &dktypenames[DKMAXTYPES]; cpp++)
				if ((s = *cpp) && streq(s, tp)) {
					lp->d_type = cpp - dktypenames;
					goto next;
				}
			v = atoi(tp);
			if ((unsigned)v >= DKMAXTYPES)
				fprintf(stderr, "line %d:%s %d\n", lineno,
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
			register int i;

			for (i = 0; (cp = tp) && *cp != '\0' && i < NDDATA;) {
				lp->d_drivedata[i++] = atoi(cp);
				tp = word(cp);
			}
			continue;
		}
		if (sscanf(cp, "%d partitions", &v) == 1) {
			if (v == 0 || (unsigned)v > MAXPARTITIONS) {
				fprintf(stderr,
				    "line %d: bad # of partitions\n", lineno);
				lp->d_npartitions = MAXPARTITIONS;
				errors++;
			} else
				lp->d_npartitions = v;
			continue;
		}
		if (tp == NULL)
			tp = "";
		if (streq(cp, "disk")) {
			strncpy(lp->d_typename, tp, sizeof (lp->d_typename));
			continue;
		}
		if (streq(cp, "label")) {
			strncpy(lp->d_packname, tp, sizeof (lp->d_packname));
			continue;
		}
		if (streq(cp, "bytes/sector")) {
			v = atoi(tp);
			if (v <= 0 || (v % 512) != 0) {
				fprintf(stderr,
				    "line %d: %s: bad sector size\n",
				    lineno, tp);
				errors++;
			} else
				lp->d_secsize = v;
			continue;
		}
		if (streq(cp, "sectors/track")) {
			v = atoi(tp);
			if (v <= 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_nsectors = v;
			continue;
		}
		if (streq(cp, "sectors/cylinder")) {
			v = atoi(tp);
			if (v <= 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_secpercyl = v;
			continue;
		}
		if (streq(cp, "tracks/cylinder")) {
			v = atoi(tp);
			if (v <= 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_ntracks = v;
			continue;
		}
		if (streq(cp, "cylinders")) {
			v = atoi(tp);
			if (v <= 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_ncylinders = v;
			continue;
		}
		if (streq(cp, "rpm")) {
			v = atoi(tp);
			if (v <= 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_rpm = v;
			continue;
		}
		if (streq(cp, "interleave")) {
			v = atoi(tp);
			if (v <= 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_interleave = v;
			continue;
		}
		if (streq(cp, "trackskew")) {
			v = atoi(tp);
			if (v < 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_trackskew = v;
			continue;
		}
		if (streq(cp, "cylinderskew")) {
			v = atoi(tp);
			if (v < 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_cylskew = v;
			continue;
		}
		if (streq(cp, "headswitch")) {
			v = atoi(tp);
			if (v < 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_headswitch = v;
			continue;
		}
		if (streq(cp, "track-to-track seek")) {
			v = atoi(tp);
			if (v < 0) {
				fprintf(stderr, "line %d: %s: bad %s\n",
				    lineno, tp, cp);
				errors++;
			} else
				lp->d_trkseek = v;
			continue;
		}
		if ('a' <= *cp && *cp <= 'z' && cp[1] == '\0') {
			unsigned part = *cp - 'a';

			if (part > lp->d_npartitions) {
				fprintf(stderr,
				    "line %d: bad partition name\n", lineno);
				errors++;
				continue;
			}
			pp = &lp->d_partitions[part];
#define NXTNUM(n) { \
	cp = tp, tp = word(cp); \
	if (tp == NULL) \
		tp = cp; \
	(n) = atoi(cp); \
     }

			NXTNUM(v);
			if (v < 0) {
				fprintf(stderr,
				    "line %d: %s: bad partition size\n",
				    lineno, cp);
				errors++;
			} else
				pp->p_size = v;
			NXTNUM(v);
			if (v < 0) {
				fprintf(stderr,
				    "line %d: %s: bad partition offset\n",
				    lineno, cp);
				errors++;
			} else
				pp->p_offset = v;
			cp = tp, tp = word(cp);
			cpp = fstypenames;
			for (; cpp < &fstypenames[FSMAXTYPES]; cpp++)
				if ((s = *cpp) && streq(s, cp)) {
					pp->p_fstype = cpp - fstypenames;
					goto gottype;
				}
			if (isdigit(*cp))
				v = atoi(cp);
			else
				v = FSMAXTYPES;
			if ((unsigned)v >= FSMAXTYPES) {
				fprintf(stderr, "line %d: %s %s\n", lineno,
				    "Warning, unknown filesystem type", cp);
				v = FS_UNUSED;
			}
			pp->p_fstype = v;
	gottype:

			switch (pp->p_fstype) {

			case FS_UNUSED:				/* XXX */
				NXTNUM(pp->p_fsize);
				if (pp->p_fsize == 0)
					break;
				NXTNUM(v);
				pp->p_frag = v / pp->p_fsize;
				break;

			case FS_BSDFFS:
				NXTNUM(pp->p_fsize);
				if (pp->p_fsize == 0)
					break;
				NXTNUM(v);
				pp->p_frag = v / pp->p_fsize;
				NXTNUM(pp->p_cpg);
				break;

			default:
				break;
			}
			continue;
		}
		fprintf(stderr, "line %d: %s: Unknown disklabel field\n",
		    lineno, cp);
		errors++;
	next:
		;
	}
	errors += checklabel(lp);
	return (errors == 0);
}

/*
 * Check disklabel for errors and fill in
 * derived fields according to supplied values.
 */
checklabel(lp)
	register struct disklabel *lp;
{
	register struct partition *pp;
	int i, errors = 0;
	char part;
	unsigned long secper;

	if (lp->d_secsize == 0) {
		fprintf(stderr, "sector size %d\n", lp->d_secsize);
		return (1);
	}
	if (lp->d_nsectors == 0) {
		fprintf(stderr, "sectors/track %d\n", lp->d_nsectors);
		return (1);
	}
	if (lp->d_ntracks == 0) {
		fprintf(stderr, "tracks/cylinder %d\n", lp->d_ntracks);
		return (1);
	}
	if  (lp->d_ncylinders == 0) {
		fprintf(stderr, "cylinders/unit %d\n", lp->d_ncylinders);
		errors++;
	}
	if (lp->d_rpm == 0)
		Warning("revolutions/minute %d\n", lp->d_rpm);
	secper = lp->d_nsectors * lp->d_ntracks;
	if (lp->d_secpercyl == 0)
		lp->d_secpercyl = secper;
	else if (lp->d_secpercyl != secper) {
		fprintf(stderr, "sectors/cylinder %lu should be %lu\n",
			lp->d_secpercyl, secper);
		errors++;
	}
	secper = lp->d_secpercyl * lp->d_ncylinders;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = secper;
	else if (lp->d_secperunit != secper) {
		/*
		 * lp->d_secperunit makes sense as a limit on the disk size
		 * independent of the product.  However, bad144 handling at
		 * least requires it to be the same as the product, and the
		 * "whole disk" partition may be used to limit the size.
		 *
		 * XXX It's silly to accept derived quantities as input only
		 * to reject them.
		 */
		fprintf(stderr, "sectors/unit %lu should be %lu\n",
			lp->d_secperunit, secper);
		errors++;
	}
#ifdef __386BSD__notyet
	if (dosdp && dosdp->dp_size && dosdp->dp_typ == DOSPTYP_386BSD
		&& lp->d_secperunit > dosdp->dp_start + dosdp->dp_size) {
		fprintf(stderr, "exceeds DOS partition size\n");
		errors++;
		lp->d_secperunit = dosdp->dp_start + dosdp->dp_size;
	}
	/* XXX should also check geometry against BIOS's idea */
#endif
	if (lp->d_bbsize == 0) {
		fprintf(stderr, "boot block size %d\n", lp->d_bbsize);
		errors++;
	} else if (lp->d_bbsize % lp->d_secsize)
		Warning("boot block size %% sector-size != 0\n");
	if (lp->d_sbsize == 0) {
		fprintf(stderr, "super block size %d\n", lp->d_sbsize);
		errors++;
	} else if (lp->d_sbsize % lp->d_secsize)
		Warning("super block size %% sector-size != 0\n");
	if (lp->d_npartitions > MAXPARTITIONS)
		Warning("number of partitions (%d) > MAXPARTITIONS (%d)\n",
		    lp->d_npartitions, MAXPARTITIONS);
	for (i = 0; i < lp->d_npartitions; i++) {
		part = 'a' + i;
		pp = &lp->d_partitions[i];
		if (pp->p_size == 0 && pp->p_offset != 0)
			Warning("partition %c: size 0, but offset %d\n",
			    part, pp->p_offset);
#ifdef notdef
		if (pp->p_size % lp->d_secpercyl)
			Warning("partition %c: size %% cylinder-size != 0\n",
			    part);
		if (pp->p_offset % lp->d_secpercyl)
			Warning("partition %c: offset %% cylinder-size != 0\n",
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
	}
	for (; i < MAXPARTITIONS; i++) {
		part = 'a' + i;
		pp = &lp->d_partitions[i];
		if (pp->p_size || pp->p_offset)
			Warning("unused partition %c: size %d offset %d\n",
			    'a' + i, pp->p_size, pp->p_offset);
	}
	return (errors);
}

/*VARARGS1*/
Warning(fmt, a1, a2, a3, a4, a5)
	char *fmt;
{

	fprintf(stderr, "Warning, ");
	fprintf(stderr, fmt, a1, a2, a3, a4, a5);
	fprintf(stderr, "\n");
}

Perror(str)
	char *str;
{
	fputs("disklabel: ", stderr); perror(str);
	exit(4);
}

usage()
{
#ifdef BOOT
	fprintf(stderr, "%-62s%s\n%-62s%s\n%-62s%s\n%-62s%s\n%-62s%s\n",
"usage: disklabel [-r] disk", "(to read label)",
"or disklabel -w [-r] disk type [ packid ] [ xxboot bootxx ]", "(to write label)",
"or disklabel -e [-r] disk", "(to edit label)",
"or disklabel -R [-r] disk protofile [ type | xxboot bootxx ]", "(to restore label)",
"or disklabel [-NW] disk", "(to write disable/enable label)");
#else
	fprintf(stderr, "%-43s%s\n%-43s%s\n%-43s%s\n%-43s%s\n%-43s%s\n",
"usage: disklabel [-r] disk", "(to read label)",
"or disklabel -w [-r] disk type [ packid ]", "(to write label)",
"or disklabel -e [-r] disk", "(to edit label)",
"or disklabel -R [-r] disk protofile", "(to restore label)",
"or disklabel [-NW] disk", "(to write disable/enable label)");
#endif
	exit(1);
}
