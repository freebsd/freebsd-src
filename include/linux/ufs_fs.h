/*
 *  linux/include/linux/ufs_fs.h
 *
 * Copyright (C) 1996
 * Adrian Rodriguez (adrian@franklins-tower.rutgers.edu)
 * Laboratory for Computer Science Research Computing Facility
 * Rutgers, The State University of New Jersey
 *
 * Clean swab support by Fare <fare@tunes.org>
 * just hope no one is using NNUUXXI on __?64 structure elements
 * 64-bit clean thanks to Maciej W. Rozycki <macro@ds2.pg.gda.pl>
 *
 * 4.4BSD (FreeBSD) support added on February 1st 1998 by
 * Niels Kristian Bech Jensen <nkbj@image.dk> partially based
 * on code by Martin von Loewis <martin@mira.isdn.cs.tu-berlin.de>.
 *
 * NeXTstep support added on February 5th 1998 by
 * Niels Kristian Bech Jensen <nkbj@image.dk>.
 *
 * Write support by Daniel Pirkl <daniel.pirkl@email.cz>
 *
 * HP/UX hfs filesystem support added by
 * Martin K. Petersen <mkp@mkp.net>, August 1999
 *
 */

#ifndef __LINUX_UFS_FS_H
#define __LINUX_UFS_FS_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/stat.h>

#define UFS_BBLOCK 0
#define UFS_BBSIZE 8192
#define UFS_SBLOCK 8192
#define UFS_SBSIZE 8192

#define UFS_SECTOR_SIZE 512
#define UFS_SECTOR_BITS 9
#define UFS_MAGIC 0x00011954
#define UFS_CIGAM 0x54190100 /* byteswapped MAGIC */


/* HP specific MAGIC values */

#define UFS_MAGIC_LFN   0x00095014 /* fs supports filenames > 14 chars */
#define UFS_CIGAM_LFN   0x14500900 /* srahc 41 < semanelif stroppus sf */

#define UFS_MAGIC_SEC   0x00612195 /* B1 security fs */
#define UFS_CIGAM_SEC   0x95216100

#define UFS_MAGIC_FEA   0x00195612 /* fs_featurebits supported */
#define UFS_CIGAM_FEA   0x12561900

#define UFS_MAGIC_4GB   0x05231994 /* fs > 4 GB && fs_featurebits */
#define UFS_CIGAM_4GB   0x94192305

/* Seems somebody at HP goofed here. B1 and lfs are both 0x2 !?! */
#define UFS_FSF_LFN     0x00000001 /* long file names */
#define UFS_FSF_B1      0x00000002 /* B1 security */
#define UFS_FSF_LFS     0x00000002 /* large files */
#define UFS_FSF_LUID    0x00000004 /* large UIDs */

/* End of HP stuff */


#define UFS_BSIZE	8192
#define UFS_MINBSIZE	4096
#define UFS_FSIZE	1024
#define UFS_MAXFRAG	(UFS_BSIZE / UFS_FSIZE)

#define UFS_NDADDR 12
#define UFS_NINDIR 3

#define UFS_IND_BLOCK	(UFS_NDADDR + 0)
#define UFS_DIND_BLOCK	(UFS_NDADDR + 1)
#define UFS_TIND_BLOCK	(UFS_NDADDR + 2)

#define UFS_NDIR_FRAGMENT (UFS_NDADDR << uspi->s_fpbshift)
#define UFS_IND_FRAGMENT (UFS_IND_BLOCK << uspi->s_fpbshift)
#define UFS_DIND_FRAGMENT (UFS_DIND_BLOCK << uspi->s_fpbshift)
#define UFS_TIND_FRAGMENT (UFS_TIND_BLOCK << uspi->s_fpbshift)

#define UFS_ROOTINO 2
#define UFS_FIRST_INO (UFS_ROOTINO + 1)

#define UFS_USEEFT  ((__u16)65535)

#define UFS_FSOK      0x7c269d38
#define UFS_FSACTIVE  ((char)0x00)
#define UFS_FSCLEAN   ((char)0x01)
#define UFS_FSSTABLE  ((char)0x02)
#define UFS_FSOSF1    ((char)0x03)	/* is this correct for DEC OSF/1? */
#define UFS_FSBAD     ((char)0xff)

/* From here to next blank line, s_flags for ufs_sb_info */
/* directory entry encoding */
#define UFS_DE_MASK		0x00000010	/* mask for the following */
#define UFS_DE_OLD		0x00000000
#define UFS_DE_44BSD		0x00000010
/* uid encoding */
#define UFS_UID_MASK		0x00000060	/* mask for the following */
#define UFS_UID_OLD		0x00000000
#define UFS_UID_44BSD		0x00000020
#define UFS_UID_EFT		0x00000040
/* superblock state encoding */
#define UFS_ST_MASK		0x00000700	/* mask for the following */
#define UFS_ST_OLD		0x00000000
#define UFS_ST_44BSD		0x00000100
#define UFS_ST_SUN		0x00000200
#define UFS_ST_SUNx86		0x00000400
/*cylinder group encoding */
#define UFS_CG_MASK		0x00003000	/* mask for the following */
#define UFS_CG_OLD		0x00000000
#define UFS_CG_44BSD		0x00002000
#define UFS_CG_SUN		0x00001000

/* fs_inodefmt options */
#define UFS_42INODEFMT	-1
#define UFS_44INODEFMT	2

/* mount options */
#define UFS_MOUNT_ONERROR		0x0000000F
#define UFS_MOUNT_ONERROR_PANIC		0x00000001
#define UFS_MOUNT_ONERROR_LOCK		0x00000002
#define UFS_MOUNT_ONERROR_UMOUNT	0x00000004
#define UFS_MOUNT_ONERROR_REPAIR	0x00000008

#define UFS_MOUNT_UFSTYPE		0x00000FF0
#define UFS_MOUNT_UFSTYPE_OLD		0x00000010
#define UFS_MOUNT_UFSTYPE_44BSD		0x00000020
#define UFS_MOUNT_UFSTYPE_SUN		0x00000040
#define UFS_MOUNT_UFSTYPE_NEXTSTEP	0x00000080
#define UFS_MOUNT_UFSTYPE_NEXTSTEP_CD	0x00000100
#define UFS_MOUNT_UFSTYPE_OPENSTEP	0x00000200
#define UFS_MOUNT_UFSTYPE_SUNx86	0x00000400
#define UFS_MOUNT_UFSTYPE_HP	        0x00000800

#define ufs_clear_opt(o,opt)	o &= ~UFS_MOUNT_##opt
#define ufs_set_opt(o,opt)	o |= UFS_MOUNT_##opt
#define ufs_test_opt(o,opt)	((o) & UFS_MOUNT_##opt)

/*
 * MINFREE gives the minimum acceptable percentage of file system
 * blocks which may be free. If the freelist drops below this level
 * only the superuser may continue to allocate blocks. This may
 * be set to 0 if no reserve of free blocks is deemed necessary,
 * however throughput drops by fifty percent if the file system
 * is run at between 95% and 100% full; thus the minimum default
 * value of fs_minfree is 5%. However, to get good clustering
 * performance, 10% is a better choice. hence we use 10% as our
 * default value. With 10% free space, fragmentation is not a
 * problem, so we choose to optimize for time.
 */
#define UFS_MINFREE         5
#define UFS_DEFAULTOPT      UFS_OPTTIME
            
/*
 * Turn file system block numbers into disk block addresses.
 * This maps file system blocks to device size blocks.
 */
#define ufs_fsbtodb(uspi, b)	((b) << (uspi)->s_fsbtodb)
#define	ufs_dbtofsb(uspi, b)	((b) >> (uspi)->s_fsbtodb)

/*
 * Cylinder group macros to locate things in cylinder groups.
 * They calc file system addresses of cylinder group data structures.
 */
#define	ufs_cgbase(c)	(uspi->s_fpg * (c))
#define ufs_cgstart(c)	(ufs_cgbase(c)  + uspi->s_cgoffset * ((c) & ~uspi->s_cgmask))
#define	ufs_cgsblock(c)	(ufs_cgstart(c) + uspi->s_sblkno)	/* super blk */
#define	ufs_cgcmin(c)	(ufs_cgstart(c) + uspi->s_cblkno)	/* cg block */
#define	ufs_cgimin(c)	(ufs_cgstart(c) + uspi->s_iblkno)	/* inode blk */
#define	ufs_cgdmin(c)	(ufs_cgstart(c) + uspi->s_dblkno)	/* 1st data */

/*
 * Macros for handling inode numbers:
 *     inode number to file system block offset.
 *     inode number to cylinder group number.
 *     inode number to file system block address.
 */
#define	ufs_inotocg(x)		((x) / uspi->s_ipg)
#define	ufs_inotocgoff(x)	((x) % uspi->s_ipg)
#define	ufs_inotofsba(x)	(ufs_cgimin(ufs_inotocg(x)) + ufs_inotocgoff(x) / uspi->s_inopf)
#define	ufs_inotofsbo(x)	((x) % uspi->s_inopf)

/*
 * Give cylinder group number for a file system block.
 * Give cylinder group block number for a file system block.
 */
#define	ufs_dtog(d)	((d) / uspi->s_fpg)
#define	ufs_dtogd(d)	((d) % uspi->s_fpg)

/*
 * Compute the cylinder and rotational position of a cyl block addr.
 */
#define ufs_cbtocylno(bno) \
	((bno) * uspi->s_nspf / uspi->s_spc)
#define ufs_cbtorpos(bno) \
	((((bno) * uspi->s_nspf % uspi->s_spc / uspi->s_nsect \
	* uspi->s_trackskew + (bno) * uspi->s_nspf % uspi->s_spc \
	% uspi->s_nsect * uspi->s_interleave) % uspi->s_nsect \
	* uspi->s_nrpos) / uspi->s_npsect)

/*
 * The following macros optimize certain frequently calculated
 * quantities by using shifts and masks in place of divisions
 * modulos and multiplications.
 */
#define ufs_blkoff(loc)		((loc) & uspi->s_qbmask)
#define ufs_fragoff(loc)	((loc) & uspi->s_qfmask)
#define ufs_lblktosize(blk)	((blk) << uspi->s_bshift)
#define ufs_lblkno(loc)		((loc) >> uspi->s_bshift)
#define ufs_numfrags(loc)	((loc) >> uspi->s_fshift)
#define ufs_blkroundup(size)	(((size) + uspi->s_qbmask) & uspi->s_bmask)
#define ufs_fragroundup(size)	(((size) + uspi->s_qfmask) & uspi->s_fmask)
#define ufs_fragstoblks(frags)	((frags) >> uspi->s_fpbshift)
#define ufs_blkstofrags(blks)	((blks) << uspi->s_fpbshift)
#define ufs_fragnum(fsb)	((fsb) & uspi->s_fpbmask)
#define ufs_blknum(fsb)		((fsb) & ~uspi->s_fpbmask)

#define	UFS_MAXNAMLEN 255
#define UFS_MAXMNTLEN 512
#define UFS_MAXCSBUFS 31
#define UFS_LINK_MAX 32000

/*
 * UFS_DIR_PAD defines the directory entries boundaries
 * (must be a multiple of 4)
 */
#define UFS_DIR_PAD			4
#define UFS_DIR_ROUND			(UFS_DIR_PAD - 1)
#define UFS_DIR_REC_LEN(name_len)	(((name_len) + 1 + 8 + UFS_DIR_ROUND) & ~UFS_DIR_ROUND)

struct ufs_timeval {
	__s32	tv_sec;
	__s32	tv_usec;
};

struct ufs_dir_entry {
	__u32  d_ino;			/* inode number of this entry */
	__u16  d_reclen;		/* length of this entry */
	union {
		__u16	d_namlen;		/* actual length of d_name */
		struct {
			__u8	d_type;		/* file type */
			__u8	d_namlen;	/* length of string in d_name */
		} d_44;
	} d_u;
	__u8	d_name[UFS_MAXNAMLEN + 1];	/* file name */
};

struct ufs_csum {
	__u32	cs_ndir;	/* number of directories */
	__u32	cs_nbfree;	/* number of free blocks */
	__u32	cs_nifree;	/* number of free inodes */
	__u32	cs_nffree;	/* number of free frags */
};

/*
 * This is the actual superblock, as it is laid out on the disk.
 */
struct ufs_super_block {
	__u32	fs_link;	/* UNUSED */
	__u32	fs_rlink;	/* UNUSED */
	__u32	fs_sblkno;	/* addr of super-block in filesys */
	__u32	fs_cblkno;	/* offset of cyl-block in filesys */
	__u32	fs_iblkno;	/* offset of inode-blocks in filesys */
	__u32	fs_dblkno;	/* offset of first data after cg */
	__u32	fs_cgoffset;	/* cylinder group offset in cylinder */
	__u32	fs_cgmask;	/* used to calc mod fs_ntrak */
	__u32	fs_time;	/* last time written -- time_t */
	__u32	fs_size;	/* number of blocks in fs */
	__u32	fs_dsize;	/* number of data blocks in fs */
	__u32	fs_ncg;		/* number of cylinder groups */
	__u32	fs_bsize;	/* size of basic blocks in fs */
	__u32	fs_fsize;	/* size of frag blocks in fs */
	__u32	fs_frag;	/* number of frags in a block in fs */
/* these are configuration parameters */
	__u32	fs_minfree;	/* minimum percentage of free blocks */
	__u32	fs_rotdelay;	/* num of ms for optimal next block */
	__u32	fs_rps;		/* disk revolutions per second */
/* these fields can be computed from the others */
	__u32	fs_bmask;	/* ``blkoff'' calc of blk offsets */
	__u32	fs_fmask;	/* ``fragoff'' calc of frag offsets */
	__u32	fs_bshift;	/* ``lblkno'' calc of logical blkno */
	__u32	fs_fshift;	/* ``numfrags'' calc number of frags */
/* these are configuration parameters */
	__u32	fs_maxcontig;	/* max number of contiguous blks */
	__u32	fs_maxbpg;	/* max number of blks per cyl group */
/* these fields can be computed from the others */
	__u32	fs_fragshift;	/* block to frag shift */
	__u32	fs_fsbtodb;	/* fsbtodb and dbtofsb shift constant */
	__u32	fs_sbsize;	/* actual size of super block */
	__u32	fs_csmask;	/* csum block offset */
	__u32	fs_csshift;	/* csum block number */
	__u32	fs_nindir;	/* value of NINDIR */
	__u32	fs_inopb;	/* value of INOPB */
	__u32	fs_nspf;	/* value of NSPF */
/* yet another configuration parameter */
	__u32	fs_optim;	/* optimization preference, see below */
/* these fields are derived from the hardware */
	union {
		struct {
			__u32	fs_npsect;	/* # sectors/track including spares */
		} fs_sun;
		struct {
			__s32	fs_state;	/* file system state time stamp */
		} fs_sunx86;
	} fs_u1;
	__u32	fs_interleave;	/* hardware sector interleave */
	__u32	fs_trackskew;	/* sector 0 skew, per track */
/* a unique id for this filesystem (currently unused and unmaintained) */
/* In 4.3 Tahoe this space is used by fs_headswitch and fs_trkseek */
/* Neither of those fields is used in the Tahoe code right now but */
/* there could be problems if they are.                            */
	__u32	fs_id[2];	/* file system id */
/* sizes determined by number of cylinder groups and their sizes */
	__u32	fs_csaddr;	/* blk addr of cyl grp summary area */
	__u32	fs_cssize;	/* size of cyl grp summary area */
	__u32	fs_cgsize;	/* cylinder group size */
/* these fields are derived from the hardware */
	__u32	fs_ntrak;	/* tracks per cylinder */
	__u32	fs_nsect;	/* sectors per track */
	__u32	fs_spc;		/* sectors per cylinder */
/* this comes from the disk driver partitioning */
	__u32	fs_ncyl;	/* cylinders in file system */
/* these fields can be computed from the others */
	__u32	fs_cpg;		/* cylinders per group */
	__u32	fs_ipg;		/* inodes per group */
	__u32	fs_fpg;		/* blocks per group * fs_frag */
/* this data must be re-computed after crashes */
	struct ufs_csum fs_cstotal;	/* cylinder summary information */
/* these fields are cleared at mount time */
	__s8	fs_fmod;	/* super block modified flag */
	__s8	fs_clean;	/* file system is clean flag */
	__s8	fs_ronly;	/* mounted read-only flag */
	__s8	fs_flags;	/* currently unused flag */
	__s8	fs_fsmnt[UFS_MAXMNTLEN];	/* name mounted on */
/* these fields retain the current block allocation info */
	__u32	fs_cgrotor;	/* last cg searched */
	__u32	fs_csp[UFS_MAXCSBUFS];	/* list of fs_cs info buffers */
	__u32	fs_maxcluster;
	__u32	fs_cpc;		/* cyl per cycle in postbl */
	__u16	fs_opostbl[16][8];	/* old rotation block list head */	
	union {
		struct {
			__s32	fs_sparecon[53];/* reserved for future constants */
			__s32	fs_reclaim;
			__s32	fs_sparecon2[1];
			__s32	fs_state;	/* file system state time stamp */
			__u32	fs_qbmask[2];	/* ~usb_bmask */
			__u32	fs_qfmask[2];	/* ~usb_fmask */
		} fs_sun;
		struct {
			__s32	fs_sparecon[53];/* reserved for future constants */
			__s32	fs_reclaim;
			__s32	fs_sparecon2[1];
			__u32	fs_npsect;	/* # sectors/track including spares */
			__u32	fs_qbmask[2];	/* ~usb_bmask */
			__u32	fs_qfmask[2];	/* ~usb_fmask */
		} fs_sunx86;
		struct {
			__s32	fs_sparecon[50];/* reserved for future constants */
			__s32	fs_contigsumsize;/* size of cluster summary array */
			__s32	fs_maxsymlinklen;/* max length of an internal symlink */
			__s32	fs_inodefmt;	/* format of on-disk inodes */
			__u32	fs_maxfilesize[2];	/* max representable file size */
			__u32	fs_qbmask[2];	/* ~usb_bmask */
			__u32	fs_qfmask[2];	/* ~usb_fmask */
			__s32	fs_state;	/* file system state time stamp */
		} fs_44;
	} fs_u2;
	__s32	fs_postblformat;	/* format of positional layout tables */
	__s32	fs_nrpos;		/* number of rotational positions */
	__s32	fs_postbloff;		/* (__s16) rotation block list head */
	__s32	fs_rotbloff;		/* (__u8) blocks for each rotation */
	__s32	fs_magic;		/* magic number */
	__u8	fs_space[1];		/* list of blocks for each rotation */
};

/*
 * Preference for optimization.
 */
#define UFS_OPTTIME	0	/* minimize allocation time */
#define UFS_OPTSPACE	1	/* minimize disk fragmentation */

/*
 * Rotational layout table format types
 */
#define UFS_42POSTBLFMT		-1	/* 4.2BSD rotational table format */
#define UFS_DYNAMICPOSTBLFMT	1	/* dynamic rotational table format */

/*
 * Convert cylinder group to base address of its global summary info.
 */
#define fs_cs(indx) \
	u.ufs_sb.s_csp[(indx) >> uspi->s_csshift][(indx) & ~uspi->s_csmask]

/*
 * Cylinder group block for a file system.
 *
 * Writable fields in the cylinder group are protected by the associated
 * super block lock fs->fs_lock.
 */
#define	CG_MAGIC	0x090255
#define ufs_cg_chkmagic(sb, ucg) \
	(fs32_to_cpu((sb), (ucg)->cg_magic) == CG_MAGIC)

/*
 * size of this structure is 172 B
 */
struct	ufs_cylinder_group {
	__u32	cg_link;		/* linked list of cyl groups */
	__u32	cg_magic;		/* magic number */
	__u32	cg_time;		/* time last written */
	__u32	cg_cgx;			/* we are the cgx'th cylinder group */
	__u16	cg_ncyl;		/* number of cyl's this cg */
	__u16	cg_niblk;		/* number of inode blocks this cg */
	__u32	cg_ndblk;		/* number of data blocks this cg */
	struct	ufs_csum cg_cs;		/* cylinder summary information */
	__u32	cg_rotor;		/* position of last used block */
	__u32	cg_frotor;		/* position of last used frag */
	__u32	cg_irotor;		/* position of last used inode */
	__u32	cg_frsum[UFS_MAXFRAG];	/* counts of available frags */
	__u32	cg_btotoff;		/* (__u32) block totals per cylinder */
	__u32	cg_boff;		/* (short) free block positions */
	__u32	cg_iusedoff;		/* (char) used inode map */
	__u32	cg_freeoff;		/* (u_char) free block map */
	__u32	cg_nextfreeoff;		/* (u_char) next available space */
	union {
		struct {
			__u32	cg_clustersumoff;	/* (u_int32) counts of avail clusters */
			__u32	cg_clusteroff;		/* (u_int8) free cluster map */
			__u32	cg_nclusterblks;	/* number of clusters this cg */
			__u32	cg_sparecon[13];	/* reserved for future use */
		} cg_44;
		__u32	cg_sparecon[16];	/* reserved for future use */
	} cg_u;
	__u8	cg_space[1];		/* space for cylinder group maps */
/* actually longer */
};

/*
 * structure of an on-disk inode
 */
struct ufs_inode {
	__u16	ui_mode;		/*  0x0 */
	__u16	ui_nlink;		/*  0x2 */
	union {
		struct {
			__u16	ui_suid;	/*  0x4 */
			__u16	ui_sgid;	/*  0x6 */
		} oldids;
		__u32	ui_inumber;		/*  0x4 lsf: inode number */
		__u32	ui_author;		/*  0x4 GNU HURD: author */
	} ui_u1;
	__u64	ui_size;		/*  0x8 */
	struct ufs_timeval ui_atime;	/* 0x10 access */
	struct ufs_timeval ui_mtime;	/* 0x18 modification */
	struct ufs_timeval ui_ctime;	/* 0x20 creation */
	union {
		struct {
			__u32	ui_db[UFS_NDADDR];/* 0x28 data blocks */
			__u32	ui_ib[UFS_NINDIR];/* 0x58 indirect blocks */
		} ui_addr;
		__u8	ui_symlink[4*(UFS_NDADDR+UFS_NINDIR)];/* 0x28 fast symlink */
	} ui_u2;
	__u32	ui_flags;		/* 0x64 immutable, append-only... */
	__u32	ui_blocks;		/* 0x68 blocks in use */
	__u32	ui_gen;			/* 0x6c like ext2 i_version, for NFS support */
	union {
		struct {
			__u32	ui_shadow;	/* 0x70 shadow inode with security data */
			__u32	ui_uid;		/* 0x74 long EFT version of uid */
			__u32	ui_gid;		/* 0x78 long EFT version of gid */
			__u32	ui_oeftflag;	/* 0x7c reserved */
		} ui_sun;
		struct {
			__u32	ui_uid;		/* 0x70 File owner */
			__u32	ui_gid;		/* 0x74 File group */
			__s32	ui_spare[2];	/* 0x78 reserved */
		} ui_44;
		struct {
			__u32	ui_uid;		/* 0x70 */
			__u32	ui_gid;		/* 0x74 */
			__u16	ui_modeh;	/* 0x78 mode high bits */
			__u16	ui_spare;	/* 0x7A unused */
			__u32	ui_trans;	/* 0x7c filesystem translator */
		} ui_hurd;
	} ui_u3;
};

/* FreeBSD has these in sys/stat.h */
/* ui_flags that can be set by a file owner */
#define UFS_UF_SETTABLE   0x0000ffff
#define UFS_UF_NODUMP     0x00000001  /* do not dump */
#define UFS_UF_IMMUTABLE  0x00000002  /* immutable (can't "change") */
#define UFS_UF_APPEND     0x00000004  /* append-only */
#define UFS_UF_OPAQUE     0x00000008  /* directory is opaque (unionfs) */
#define UFS_UF_NOUNLINK   0x00000010  /* can't be removed or renamed */
/* ui_flags that only root can set */
#define UFS_SF_SETTABLE   0xffff0000
#define UFS_SF_ARCHIVED   0x00010000  /* archived */
#define UFS_SF_IMMUTABLE  0x00020000  /* immutable (can't "change") */
#define UFS_SF_APPEND     0x00040000  /* append-only */
#define UFS_SF_NOUNLINK   0x00100000  /* can't be removed or renamed */

#ifdef __KERNEL__

/* balloc.c */
extern void ufs_free_fragments (struct inode *, unsigned, unsigned);
extern void ufs_free_blocks (struct inode *, unsigned, unsigned);
extern unsigned ufs_new_fragments (struct inode *, u32 *, unsigned, unsigned, unsigned, int *);

/* cylinder.c */
extern struct ufs_cg_private_info * ufs_load_cylinder (struct super_block *, unsigned);
extern void ufs_put_cylinder (struct super_block *, unsigned);

/* dir.c */
extern struct inode_operations ufs_dir_inode_operations;
extern int ufs_check_dir_entry (const char *, struct inode *, struct ufs_dir_entry *, struct buffer_head *, unsigned long);
extern int ufs_add_link (struct dentry *, struct inode *);
extern ino_t ufs_inode_by_name(struct inode *, struct dentry *);
extern int ufs_make_empty(struct inode *, struct inode *);
extern struct ufs_dir_entry * ufs_find_entry (struct dentry *, struct buffer_head **);
extern int ufs_delete_entry (struct inode *, struct ufs_dir_entry *, struct buffer_head *);
extern int ufs_empty_dir (struct inode *);
extern struct ufs_dir_entry * ufs_dotdot (struct inode *, struct buffer_head **);
extern void ufs_set_link(struct inode *, struct ufs_dir_entry *, struct buffer_head *, struct inode *);

/* file.c */
extern struct inode_operations ufs_file_inode_operations;
extern struct file_operations ufs_file_operations;

extern struct address_space_operations ufs_aops;

/* ialloc.c */
extern void ufs_free_inode (struct inode *inode);
extern struct inode * ufs_new_inode (const struct inode *, int);

/* inode.c */
extern int ufs_frag_map (struct inode *, int);
extern void ufs_read_inode (struct inode *);
extern void ufs_put_inode (struct inode *);
extern void ufs_write_inode (struct inode *, int);
extern int ufs_sync_inode (struct inode *);
extern void ufs_delete_inode (struct inode *);
extern struct buffer_head * ufs_getfrag (struct inode *, unsigned, int, int *);
extern struct buffer_head * ufs_bread (struct inode *, unsigned, int, int *);

/* namei.c */
extern struct file_operations ufs_dir_operations;
        
/* super.c */
extern struct file_system_type ufs_fs_type;
extern void ufs_warning (struct super_block *, const char *, const char *, ...) __attribute__ ((format (printf, 3, 4)));
extern void ufs_error (struct super_block *, const char *, const char *, ...) __attribute__ ((format (printf, 3, 4)));
extern void ufs_panic (struct super_block *, const char *, const char *, ...) __attribute__ ((format (printf, 3, 4)));
extern void ufs_write_super (struct super_block *);

/* symlink.c */
extern struct inode_operations ufs_fast_symlink_inode_operations;

/* truncate.c */
extern void ufs_truncate (struct inode *);

#endif	/* __KERNEL__ */

#endif /* __LINUX_UFS_FS_H */
