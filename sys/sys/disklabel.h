/*
 * Copyright (c) 1987, 1988 Regents of the University of California.
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
 *
 *	from: @(#)disklabel.h	7.19 (Berkeley) 5/7/91
 *	$Id: disklabel.h,v 1.6 1993/12/19 00:55:13 wollman Exp $
 */

#ifndef _SYS_DISKLABEL_H_
#define _SYS_DISKLABEL_H_ 1

#ifndef KERNEL
#include <sys/cdefs.h>
#include <sys/conf.h>
#else
#include "conf.h"
#endif

/*
 * Disk description table, see disktab(5)
 */
#define	_PATH_DISKTAB	"/etc/disktab"
#define	DISKTAB		"/etc/disktab"		/* deprecated */

/*
 * Each disk has a label which includes information about the hardware
 * disk geometry, filesystem partitions, and drive specific information.
 * The label is in block 0 or 1, possibly offset from the beginning
 * to leave room for a bootstrap, etc.
 */

/* XXX these should be defined per controller (or drive) elsewhere, not here! */
#ifdef i386
#define LABELSECTOR	1			/* sector containing label */
#define LABELOFFSET	0			/* offset of label in sector */
#endif

#ifndef	LABELSECTOR
#define LABELSECTOR	0			/* sector containing label */
#endif

#ifndef	LABELOFFSET
#define LABELOFFSET	64			/* offset of label in sector */
#endif

#define DISKMAGIC	(0x82564557UL)	/* The disk magic number */
#ifndef MAXPARTITIONS
#define	MAXPARTITIONS	8
#endif


#ifndef LOCORE
struct disklabel {
	u_long	d_magic;		/* the magic number */
	short	d_type;			/* drive type */
	short	d_subtype;		/* controller/d_type specific */
	char	d_typename[16];		/* type name, e.g. "eagle" */
	/* 
	 * d_packname contains the pack identifier and is returned when
	 * the disklabel is read off the disk or in-core copy.
	 * d_boot0 and d_boot1 are the (optional) names of the
	 * primary (block 0) and secondary (block 1-15) bootstraps
	 * as found in /usr/mdec.  These are returned when using
	 * getdiskbyname(3) to retrieve the values from /etc/disktab.
	 */
#if defined(KERNEL) || defined(STANDALONE)
	char	d_packname[16];			/* pack identifier */ 
#else
	union {
		char	un_d_packname[16];	/* pack identifier */ 
		struct {
			char *un_d_boot0;	/* primary bootstrap name */
			char *un_d_boot1;	/* secondary bootstrap name */
		} un_b; 
	} d_un; 
#define d_packname	d_un.un_d_packname
#define d_boot0		d_un.un_b.un_d_boot0
#define d_boot1		d_un.un_b.un_d_boot1
#endif	/* ! KERNEL or STANDALONE */
			/* disk geometry: */
	u_long	d_secsize;		/* # of bytes per sector */
	u_long	d_nsectors;		/* # of data sectors per track */
	u_long	d_ntracks;		/* # of tracks per cylinder */
	u_long	d_ncylinders;		/* # of data cylinders per unit */
	u_long	d_secpercyl;		/* # of data sectors per cylinder */
	u_long	d_secperunit;		/* # of data sectors per unit */
	/*
	 * Spares (bad sector replacements) below
	 * are not counted in d_nsectors or d_secpercyl.
	 * Spare sectors are assumed to be physical sectors
	 * which occupy space at the end of each track and/or cylinder.
	 */
	u_short	d_sparespertrack;	/* # of spare sectors per track */
	u_short	d_sparespercyl;		/* # of spare sectors per cylinder */
	/*
	 * Alternate cylinders include maintenance, replacement,
	 * configuration description areas, etc.
	 */
	u_long	d_acylinders;		/* # of alt. cylinders per unit */

			/* hardware characteristics: */
	/*
	 * d_interleave, d_trackskew and d_cylskew describe perturbations
	 * in the media format used to compensate for a slow controller.
	 * Interleave is physical sector interleave, set up by the formatter
	 * or controller when formatting.  When interleaving is in use,
	 * logically adjacent sectors are not physically contiguous,
	 * but instead are separated by some number of sectors.
	 * It is specified as the ratio of physical sectors traversed
	 * per logical sector.  Thus an interleave of 1:1 implies contiguous
	 * layout, while 2:1 implies that logical sector 0 is separated
	 * by one sector from logical sector 1.
	 * d_trackskew is the offset of sector 0 on track N
	 * relative to sector 0 on track N-1 on the same cylinder.
	 * Finally, d_cylskew is the offset of sector 0 on cylinder N
	 * relative to sector 0 on cylinder N-1.
	 */
	u_short	d_rpm;			/* rotational speed */
	u_short	d_interleave;		/* hardware sector interleave */
	u_short	d_trackskew;		/* sector 0 skew, per track */
	u_short	d_cylskew;		/* sector 0 skew, per cylinder */
	u_long	d_headswitch;		/* head switch time, usec */
	u_long	d_trkseek;		/* track-to-track seek, usec */
	u_long	d_flags;		/* generic flags */
#define NDDATA 5
	u_long	d_drivedata[NDDATA];	/* drive-type specific information */
#define NSPARE 5
	u_long	d_spare[NSPARE];	/* reserved for future use */
	u_long	d_magic2;		/* the magic number (again) */
	u_short	d_checksum;		/* xor of data incl. partitions */

			/* filesystem and partition information: */
	u_short	d_npartitions;		/* number of partitions in following */
	u_long	d_bbsize;		/* size of boot area at sn0, bytes */
	u_long	d_sbsize;		/* max size of fs superblock, bytes */
	struct	partition {		/* the partition table */
		u_long	p_size;		/* number of sectors in partition */
		u_long	p_offset;	/* starting sector */
		u_long	p_fsize;	/* filesystem basic fragment size */
		u_char	p_fstype;	/* filesystem type, see below */
		u_char	p_frag;		/* filesystem fragments per block */
		u_short	p_cpg;		/* filesystem cylinders per group */
	} d_partitions[MAXPARTITIONS];	/* actually may be more */
};
#else /* LOCORE */
	/*
	 * offsets for asm boot files.
	 */
	.set	d_secsize,40
	.set	d_nsectors,44
	.set	d_ntracks,48
	.set	d_ncylinders,52
	.set	d_secpercyl,56
	.set	d_secperunit,60
	.set	d_end_,276		/* size of disk label */
#endif /* LOCORE*/

/* d_type values: */
#define	DTYPE_SMD		1		/* SMD, XSMD; VAX hp/up */
#define	DTYPE_MSCP		2		/* MSCP */
#define	DTYPE_DEC		3		/* other DEC (rk, rl) */
#define	DTYPE_SCSI		4		/* SCSI */
#define	DTYPE_ESDI		5		/* ESDI interface */
#define	DTYPE_ST506		6		/* ST506 etc. */
#define	DTYPE_FLOPPY		10		/* floppy */

/* d_subtype values: */
#define DSTYPE_INDOSPART	0x8		/* is inside dos partition */
#define  DSTYPE_DOSPART(s)	 ((s) & 3)	 /* dos partition number */
#define DSTYPE_GEOMETRY		0x10		/* drive params in label */

#ifdef DKTYPENAMES
static char *dktypenames[] = {
	"unknown",
	"SMD",
	"MSCP",
	"old DEC",
	"SCSI",
	"ESDI",
	"ST506",
	"type 7",
	"type 8",
	"type 9",
	"floppy",
	0
};
#define DKMAXTYPES	(sizeof(dktypenames) / sizeof(dktypenames[0]) - 1)
#endif

/*
 * Filesystem type and version.
 * Used to interpret other filesystem-specific
 * per-partition information.
 */
#define	FS_UNUSED	0		/* unused */
#define	FS_SWAP		1		/* swap */
#define	FS_V6		2		/* Sixth Edition */
#define	FS_V7		3		/* Seventh Edition */
#define	FS_SYSV		4		/* System V */
#define	FS_V71K		5		/* V7 with 1K blocks (4.1, 2.9) */
#define	FS_V8		6		/* Eighth Edition, 4K blocks */
#define	FS_BSDFFS	7		/* 4.2BSD fast file system */
#define	FS_MSDOS	8		/* MSDOS file system */

#ifdef	DKTYPENAMES
static char *fstypenames[] = {
	"unused",
	"swap",
	"Version 6",
	"Version 7",
	"System V",
	"4.1BSD",
	"Eighth Edition",
	"4.2BSD",
	"MSDOS",
	0
};
#define FSMAXTYPES	(sizeof(fstypenames) / sizeof(fstypenames[0]) - 1)
#endif

/*
 * flags shared by various drives:
 */
#define		D_REMOVABLE	0x01		/* removable media */
#define		D_ECC		0x02		/* supports ECC */
#define		D_BADSECT	0x04		/* supports bad sector forw. */
#define		D_RAMDISK	0x08		/* disk emulator */
#define		D_CHAIN		0x10		/* can do back-back transfers */
#define		D_DOSPART	0x20		/* within MSDOS partition */

/*
 * Drive data for SMD.
 */
#define	d_smdflags	d_drivedata[0]
#define		D_SSE		0x1		/* supports skip sectoring */
#define	d_mindist	d_drivedata[1]
#define	d_maxdist	d_drivedata[2]
#define	d_sdist		d_drivedata[3]

/*
 * Drive data for ST506.
 */
#define d_precompcyl	d_drivedata[0]
#define d_gap3		d_drivedata[1]		/* used only when formatting */

/*
 * Drive data for SCSI.
 */
#define	d_blind		d_drivedata[0]

#ifndef LOCORE
/*
 * Structure used to perform a format
 * or other raw operation, returning data
 * and/or register values.
 * Register identification and format
 * are device- and driver-dependent.
 */
struct format_op {
	char	*df_buf;
	int	df_count;		/* value-result */
	daddr_t	df_startblk;
	int	df_reg[8];		/* result */
};

/*
 * Structure used internally to retrieve
 * information about a partition on a disk.
 */
struct partinfo {
	struct disklabel *disklab;
	struct partition *part;
};

/* DOS partition table -- located in boot block */

#define	DOSBBSECTOR	0	/* DOS boot block relative sector number */
#define	DOSPARTOFF	446
#define NDOSPART	4

struct dos_partition {
	unsigned char	dp_flag;	/* bootstrap flags */
	unsigned char	dp_shd;		/* starting head */
	unsigned char	dp_ssect;	/* starting sector */
	unsigned char	dp_scyl;	/* starting cylinder */
	unsigned char	dp_typ;		/* partition type */
#define		DOSPTYP_386BSD	0xa5		/* 386BSD partition type */
	unsigned char	dp_ehd;		/* end head */
	unsigned char	dp_esect;	/* end sector */
	unsigned char	dp_ecyl;	/* end cylinder */
	unsigned long	dp_start;	/* absolute starting sector number */
	unsigned long	dp_size;	/* partition size in sectors */
};

extern struct dos_partition dos_partitions[NDOSPART];

#define	DPSECT(s) ((s) & 0x3f)		/* isolate relevant bits of sector */
#define	DPCYL(c, s) ((c) + (((s) & 0xc0)<<2)) /* and those that are cylinder */

/*
 * Disk-specific ioctls.
 */
		/* get and set disklabel; DIOCGPART used internally */
#define DIOCGDINFO	_IOR('d', 101, struct disklabel)/* get */
#define DIOCSDINFO	_IOW('d', 102, struct disklabel)/* set */
#define DIOCWDINFO	_IOW('d', 103, struct disklabel)/* set, update disk */
#define DIOCGPART	_IOW('d', 104, struct partinfo)	/* get partition */

/* do format operation, read or write */
#define DIOCRFORMAT	_IOWR('d', 105, struct format_op)
#define DIOCWFORMAT	_IOWR('d', 106, struct format_op)

#define DIOCSSTEP	_IOW('d', 107, int)	/* set step rate */
#define DIOCSRETRIES	_IOW('d', 108, int)	/* set # of retries */
#define DIOCWLABEL	_IOW('d', 109, int)	/* write en/disable label */

#define DIOCSBAD	_IOW('d', 110, struct dkbad)	/* set kernel dkbad */

#if defined(KERNEL)
void diskerr(struct buf *, char *, char *, int, int, struct disklabel *);
int dkcksum(struct disklabel *);

int setdisklabel(struct disklabel *, struct disklabel *, u_long,
	struct dos_partition *);

char *readdisklabel(int, d_strategy_t *, struct disklabel *,
	struct dos_partition *, struct dkbad *, struct buf **);

void disksort(struct buf *, struct buf *);

int writedisklabel(int, d_strategy_t *, struct disklabel *,
		struct dos_partition *);

int bounds_check_with_label(struct buf *, struct disklabel *, int);
#endif
#endif /* LOCORE */

#if !defined(KERNEL) && !defined(LOCORE)

#include <sys/cdefs.h>

__BEGIN_DECLS
struct disklabel *getdiskbyname __P((const char *));
__END_DECLS

#endif
#endif /* _SYS_DISKLABEL_H_ */
