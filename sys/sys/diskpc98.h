/*
 * Copyright (c) 1987, 1988, 1993
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
 *
 *	@(#)disklabel.h	8.2 (Berkeley) 7/10/94
 * $FreeBSD$
 */

#ifndef _SYS_DISKLABEL_H_
#define	_SYS_DISKLABEL_H_

#ifndef _KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>

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
#ifdef __i386__
#define LABELSECTOR	1			/* sector containing label */
#define LABELOFFSET	0			/* offset of label in sector */
#endif

#ifdef __alpha__
#define LABELSECTOR	0
#define LABELOFFSET	64
#endif

#ifdef __ia64__
#define LABELSECTOR	1
#define LABELOFFSET	0
#endif

#ifdef __sparc64__
#define LABELSECTOR	0
#define LABELOFFSET	128
#endif

#ifndef	LABELSECTOR
#define LABELSECTOR	0
#endif

#ifndef	LABELOFFSET
#define LABELOFFSET	64
#endif

#define DISKMAGIC	((u_int32_t)0x82564557)	/* The disk magic number */
#ifndef MAXPARTITIONS
#define	MAXPARTITIONS	8
#endif

#define	LABEL_PART	2		/* partition containing label */
#define	RAW_PART	2		/* partition containing whole disk */
#define	SWAP_PART	1		/* partition normally containing swap */

#ifndef LOCORE
struct disklabel {
	u_int32_t d_magic;		/* the magic number */
	u_int16_t d_type;		/* drive type */
	u_int16_t d_subtype;		/* controller/d_type specific */
	char	  d_typename[16];	/* type name, e.g. "eagle" */

	/* 
	 * d_packname contains the pack identifier and is returned when
	 * the disklabel is read off the disk or in-core copy.
	 * d_boot0 and d_boot1 are the (optional) names of the
	 * primary (block 0) and secondary (block 1-15) bootstraps
	 * as found in /boot.  These are returned when using
	 * getdiskbyname(3) to retrieve the values from /etc/disktab.
	 */
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

			/* disk geometry: */
	u_int32_t d_secsize;		/* # of bytes per sector */
	u_int32_t d_nsectors;		/* # of data sectors per track */
	u_int32_t d_ntracks;		/* # of tracks per cylinder */
	u_int32_t d_ncylinders;		/* # of data cylinders per unit */
	u_int32_t d_secpercyl;		/* # of data sectors per cylinder */
	u_int32_t d_secperunit;		/* # of data sectors per unit */

	/*
	 * Spares (bad sector replacements) below are not counted in
	 * d_nsectors or d_secpercyl.  Spare sectors are assumed to
	 * be physical sectors which occupy space at the end of each
	 * track and/or cylinder.
	 */
	u_int16_t d_sparespertrack;	/* # of spare sectors per track */
	u_int16_t d_sparespercyl;	/* # of spare sectors per cylinder */
	/*
	 * Alternate cylinders include maintenance, replacement, configuration
	 * description areas, etc.
	 */
	u_int32_t d_acylinders;		/* # of alt. cylinders per unit */

			/* hardware characteristics: */
	/*
	 * d_interleave, d_trackskew and d_cylskew describe perturbations
	 * in the media format used to compensate for a slow controller.
	 * Interleave is physical sector interleave, set up by the
	 * formatter or controller when formatting.  When interleaving is
	 * in use, logically adjacent sectors are not physically
	 * contiguous, but instead are separated by some number of
	 * sectors.  It is specified as the ratio of physical sectors
	 * traversed per logical sector.  Thus an interleave of 1:1
	 * implies contiguous layout, while 2:1 implies that logical
	 * sector 0 is separated by one sector from logical sector 1.
	 * d_trackskew is the offset of sector 0 on track N relative to
	 * sector 0 on track N-1 on the same cylinder.  Finally, d_cylskew
	 * is the offset of sector 0 on cylinder N relative to sector 0
	 * on cylinder N-1.
	 */
	u_int16_t d_rpm;		/* rotational speed */
	u_int16_t d_interleave;		/* hardware sector interleave */
	u_int16_t d_trackskew;		/* sector 0 skew, per track */
	u_int16_t d_cylskew;		/* sector 0 skew, per cylinder */
	u_int32_t d_headswitch;		/* head switch time, usec */
	u_int32_t d_trkseek;		/* track-to-track seek, usec */
	u_int32_t d_flags;		/* generic flags */
#define NDDATA 5
	u_int32_t d_drivedata[NDDATA];	/* drive-type specific information */
#define NSPARE 5
	u_int32_t d_spare[NSPARE];	/* reserved for future use */
	u_int32_t d_magic2;		/* the magic number (again) */
	u_int16_t d_checksum;		/* xor of data incl. partitions */

			/* filesystem and partition information: */
	u_int16_t d_npartitions;	/* number of partitions in following */
	u_int32_t d_bbsize;		/* size of boot area at sn0, bytes */
	u_int32_t d_sbsize;		/* max size of fs superblock, bytes */
	struct partition {		/* the partition table */
		u_int32_t p_size;	/* number of sectors in partition */
		u_int32_t p_offset;	/* starting sector */
		u_int32_t p_fsize;	/* filesystem basic fragment size */
		u_int8_t p_fstype;	/* filesystem type, see below */
		u_int8_t p_frag;	/* filesystem fragments per block */
		u_int16_t p_cpg;	/* filesystem cylinders per group */
	} d_partitions[MAXPARTITIONS];	/* actually may be more */
};

static __inline u_int16_t dkcksum(struct disklabel *lp);
static __inline u_int16_t
dkcksum(lp)
	struct disklabel *lp;
{
	u_int16_t *start, *end;
	u_int16_t sum = 0;

	start = (u_int16_t *)lp;
	end = (u_int16_t *)&lp->d_partitions[lp->d_npartitions];
	while (start < end)
		sum ^= *start++;
	return (sum);
}

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
#endif /* LOCORE */

/* d_type values: */
#define	DTYPE_SMD		1		/* SMD, XSMD; VAX hp/up */
#define	DTYPE_MSCP		2		/* MSCP */
#define	DTYPE_DEC		3		/* other DEC (rk, rl) */
#define	DTYPE_SCSI		4		/* SCSI */
#define	DTYPE_ESDI		5		/* ESDI interface */
#define	DTYPE_ST506		6		/* ST506 etc. */
#define	DTYPE_HPIB		7		/* CS/80 on HP-IB */
#define	DTYPE_HPFL		8		/* HP Fiber-link */
#define	DTYPE_FLOPPY		10		/* floppy */
#define	DTYPE_CCD		11		/* concatenated disk */
#define	DTYPE_VINUM		12		/* vinum volume */
#define	DTYPE_DOC2K		13		/* Msys DiskOnChip */
#define	DTYPE_JFS2		16		/* IBM JFS 2 */

#if defined(PC98) && !defined(PC98_ATCOMPAT)
#define	DSTYPE_SEC256		0x80		/* physical sector size=256 */
#endif

#ifdef DKTYPENAMES
static char *dktypenames[] = {
	"unknown",
	"SMD",
	"MSCP",
	"old DEC",
	"SCSI",
	"ESDI",
	"ST506",
	"HP-IB",
	"HP-FL",
	"type 9",
	"floppy",
	"CCD",
	"Vinum",
	"DOC2K",
	"?",
	"?",
	"jfs",
	NULL
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
#define	FS_BSDLFS	9		/* 4.4BSD log-structured file system */
#define	FS_OTHER	10		/* in use, but unknown/unsupported */
#define	FS_HPFS		11		/* OS/2 high-performance file system */
#define	FS_ISO9660	12		/* ISO 9660, normally CD-ROM */
#define	FS_BOOT		13		/* partition contains bootstrap */
#define	FS_VINUM	14		/* Vinum drive */
#define	FS_JFS2		21		/* IBM JFS2 */

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
	"4.4LFS",
	"unknown",
	"HPFS",
	"ISO9660",
	"boot",
	"vinum",
	"?",
	"?",
	"?",
	"?",
	"?",
	"?",
	"?",
	"jfs",
	NULL
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
 * Structure used to perform a format or other raw operation, returning
 * data and/or register values.  Register identification and format
 * are device- and driver-dependent.
 */
struct format_op {
	char	*df_buf;
	int	 df_count;		/* value-result */
	daddr_t	 df_startblk;
	int	 df_reg[8];		/* result */
};

#ifdef _KERNEL
/*
 * Structure used internally to retrieve information about a partition
 * on a disk.
 */
struct partinfo {
	struct disklabel *disklab;
	struct partition *part;
};
#endif

/* DOS partition table -- located in boot block */

#if defined(PC98) && !defined(PC98_ATCOMPAT)
#define	DOSBBSECTOR	0	/* DOS boot block relative sector number */
#define DOSLABELSECTOR	1	/* 0: 256b/s, 1: 512b/s */
#define	DOSPARTOFF	0
#define NDOSPART	16
#define	DOSPTYP_386BSD	0x94	/* 386BSD partition type */
#define	MBR_PTYPE_FreeBSD 0x94	/* FreeBSD partition type */

struct dos_partition {
    	unsigned char	dp_mid;
#define DOSMID_386BSD		(0x14|0x80) /* 386bsd|bootable */
	unsigned char	dp_sid;
#define DOSSID_386BSD		(0x44|0x80) /* 386bsd|active */	
	unsigned char	dp_dum1;
	unsigned char	dp_dum2;
	unsigned char	dp_ipl_sct;
	unsigned char	dp_ipl_head;
	unsigned short	dp_ipl_cyl;
	unsigned char	dp_ssect;	/* starting sector */
	unsigned char	dp_shd;		/* starting head */
	unsigned short	dp_scyl;	/* starting cylinder */
	unsigned char	dp_esect;	/* end sector */
	unsigned char	dp_ehd;		/* end head */
	unsigned short	dp_ecyl;	/* end cylinder */
	unsigned char	dp_name[16];
};

#else /* IBMPC */
#define DOSBBSECTOR	0	/* DOS boot block relative sector number */
#define DOSPARTOFF	446
#define NDOSPART	4
#define	DOSPTYP_386BSD	0xa5	/* 386BSD partition type */
#define	DOSPTYP_LINSWP	0x82	/* Linux swap partition */
#define	DOSPTYP_LINUX	0x83	/* Linux partition */
#define	DOSPTYP_EXT	5	/* DOS extended partition */

struct dos_partition {
	unsigned char	dp_flag;	/* bootstrap flags */
	unsigned char	dp_shd;		/* starting head */
	unsigned char	dp_ssect;	/* starting sector */
	unsigned char	dp_scyl;	/* starting cylinder */
	unsigned char	dp_typ;		/* partition type */
	unsigned char	dp_ehd;		/* end head */
	unsigned char	dp_esect;	/* end sector */
	unsigned char	dp_ecyl;	/* end cylinder */
	u_int32_t	dp_start;	/* absolute starting sector number */
	u_int32_t	dp_size;	/* partition size in sectors */
};
#endif

#define DPSECT(s) ((s) & 0x3f)		/* isolate relevant bits of sector */
#define DPCYL(c, s) ((c) + (((s) & 0xc0)<<2)) /* and those that are cylinder */

/*
 * Disk-specific ioctls.
 */
		/* get and set disklabel; DIOCGPART used internally */
#define DIOCGDINFO	_IOR('d', 101, struct disklabel)/* get */
#define DIOCSDINFO	_IOW('d', 102, struct disklabel)/* set */
#define DIOCWDINFO	_IOW('d', 103, struct disklabel)/* set, update disk */
#ifdef _KERNEL
#define DIOCGPART	_IOW('d', 104, struct partinfo)	/* get partition */
#endif
#define DIOCGDVIRGIN	_IOR('d', 105, struct disklabel)/* get virgin label */

#define DIOCWLABEL	_IOW('d', 109, int)	/* write en/disable label */

#define DIOCGSECTORSIZE	_IOR('d', 128, u_int)	/* Get sector size in bytes */
#define DIOCGMEDIASIZE	_IOR('d', 129, off_t)	/* Get media size in bytes */
#define DIOCGFWSECTORS	_IOR('d', 130, u_int)	/* Get firmware sectorcount */
#define DIOCGFWHEADS	_IOR('d', 131, u_int)	/* Get firmware headcount */
#define DIOCGFWCYLINDERS _IOR('d', 132, u_int)	/* Get firmware cyl'scount */

#ifdef _KERNEL

/*
 * XXX encoding of disk minor numbers, should be elsewhere.
 *
 * See <sys/reboot.h> for a possibly better encoding.
 *
 * "cpio -H newc" can be used to back up device files with large minor
 * numbers (but not ones >= 2^31).  Old cpio formats and all tar formats
 * don't have enough bits, and cpio and tar don't notice the lossage.
 * There are also some sign extension bugs.
 */

/*
       3                   2                   1                   0
     1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
    _________________________________________________________________
    | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
    -----------------------------------------------------------------
    |    SPARE    |UNIT_2 | SLICE   |  MAJOR?       |  UNIT   |PART |
    -----------------------------------------------------------------
*/

#define	DKMAXUNIT	0x1ff

#define	dkmakeminor(unit, slice, part) \
				(((slice) << 16) | (((unit) & 0x1e0) << 16) | \
				(((unit) & 0x1f) << 3) | (part))
#define	dkpart(dev)		(minor(dev) & 7)
#define	dkslice(dev)		((minor(dev) >> 16) & 0x1f)
#define	dksparebits(dev)       	((minor(dev) >> 25) & 0x7f)

struct	bio;
struct	bio_queue_head;

int	bounds_check_with_label(struct bio *bp, struct disklabel *lp,
	    int wlabel);
void	diskerr(struct bio *bp, char *what, int blkdone, struct disklabel *lp);
dev_t	dkmodpart(dev_t dev, int part);
dev_t	dkmodslice(dev_t dev, int slice);
u_int	dkunit(dev_t dev);
char	*readdisklabel(dev_t dev, struct disklabel *lp);
void	bioqdisksort(struct bio_queue_head *ap, struct bio *bp);
int	setdisklabel(struct disklabel *olp, struct disklabel *nlp,
	    u_long openmask);
int	writedisklabel(dev_t dev, struct disklabel *lp);
#ifdef __alpha__
struct	buf;			
void	alpha_fix_srm_checksum(struct buf *bp);
#endif

#endif /* _KERNEL */

#endif /* LOCORE */

#ifndef _KERNEL
__BEGIN_DECLS
struct disklabel *getdiskbyname(const char *);
__END_DECLS
#endif

#endif /* !_SYS_DISKLABEL_H_ */
