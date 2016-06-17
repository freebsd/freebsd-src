/* 
 *  linux/include/linux/ufs_fs_sb.h
 *
 * Copyright (C) 1996
 * Adrian Rodriguez (adrian@franklins-tower.rutgers.edu)
 * Laboratory for Computer Science Research Computing Facility
 * Rutgers, The State University of New Jersey
 *
 * $Id: ufs_fs_sb.h,v 1.8 1998/05/06 12:04:40 jj Exp $
 *
 * Write support by Daniel Pirkl <daniel.pirkl@email.cz>
 */

#ifndef __LINUX_UFS_FS_SB_H
#define __LINUX_UFS_FS_SB_H

#include <linux/ufs_fs.h>

/*
 * This structure is used for reading disk structures larger
 * than the size of fragment.
 */
struct ufs_buffer_head {
	unsigned fragment;			/* first fragment */
	unsigned count;				/* number of fragments */
	struct buffer_head * bh[UFS_MAXFRAG];	/* buffers */
};

struct ufs_cg_private_info {
	struct ufs_cylinder_group ucg;
	__u32	c_cgx;		/* number of cylidner group */
	__u16	c_ncyl;		/* number of cyl's this cg */
	__u16	c_niblk;	/* number of inode blocks this cg */
	__u32	c_ndblk;	/* number of data blocks this cg */
	__u32	c_rotor;	/* position of last used block */
	__u32	c_frotor;	/* position of last used frag */
	__u32	c_irotor;	/* position of last used inode */
	__u32	c_btotoff;	/* (__u32) block totals per cylinder */
	__u32	c_boff;		/* (short) free block positions */
	__u32	c_iusedoff;	/* (char) used inode map */
	__u32	c_freeoff;	/* (u_char) free block map */
	__u32	c_nextfreeoff;	/* (u_char) next available space */
	__u32	c_clustersumoff;/* (u_int32) counts of avail clusters */
	__u32	c_clusteroff;	/* (u_int8) free cluster map */
	__u32	c_nclusterblks;	/* number of clusters this cg */
};	

struct ufs_sb_private_info {
	struct ufs_buffer_head s_ubh; /* buffer containing super block */
	__u32	s_sblkno;	/* offset of super-blocks in filesys */
	__u32	s_cblkno;	/* offset of cg-block in filesys */
	__u32	s_iblkno;	/* offset of inode-blocks in filesys */
	__u32	s_dblkno;	/* offset of first data after cg */
	__u32	s_cgoffset;	/* cylinder group offset in cylinder */
	__u32	s_cgmask;	/* used to calc mod fs_ntrak */
	__u32	s_size;		/* number of blocks (fragments) in fs */
	__u32	s_dsize;	/* number of data blocks in fs */
	__u32	s_ncg;		/* number of cylinder groups */
	__u32	s_bsize;	/* size of basic blocks */
	__u32	s_fsize;	/* size of fragments */
	__u32	s_fpb;		/* fragments per block */
	__u32	s_minfree;	/* minimum percentage of free blocks */
	__u32	s_bmask;	/* `blkoff'' calc of blk offsets */
	__u32	s_fmask;	/* s_fsize mask */
	__u32	s_bshift;	/* `lblkno'' calc of logical blkno */
	__u32   s_fshift;	/* s_fsize shift */
	__u32	s_fpbshift;	/* fragments per block shift */
	__u32	s_fsbtodb;	/* fsbtodb and dbtofsb shift constant */
	__u32	s_sbsize;	/* actual size of super block */
	__u32   s_csmask;	/* csum block offset */
	__u32	s_csshift;	/* csum block number */
	__u32	s_nindir;	/* value of NINDIR */
	__u32	s_inopb;	/* value of INOPB */
	__u32	s_nspf;		/* value of NSPF */
	__u32	s_npsect;	/* # sectors/track including spares */
	__u32	s_interleave;	/* hardware sector interleave */
	__u32	s_trackskew;	/* sector 0 skew, per track */
	__u32	s_csaddr;	/* blk addr of cyl grp summary area */
	__u32	s_cssize;	/* size of cyl grp summary area */
	__u32	s_cgsize;	/* cylinder group size */
	__u32	s_ntrak;	/* tracks per cylinder */
	__u32	s_nsect;	/* sectors per track */
	__u32	s_spc;		/* sectors per cylinder */
	__u32	s_ipg;		/* inodes per group */
	__u32	s_fpg;		/* fragments per group */
	__u32	s_cpc;		/* cyl per cycle in postbl */
	__s32	s_contigsumsize;/* size of cluster summary array, 44bsd */
	__s64	s_qbmask;	/* ~usb_bmask */
	__s64	s_qfmask;	/* ~usb_fmask */
	__s32	s_postblformat;	/* format of positional layout tables */
	__s32	s_nrpos;	/* number of rotational positions */
        __s32	s_postbloff;	/* (__s16) rotation block list head */
	__s32	s_rotbloff;	/* (__u8) blocks for each rotation */

	__u32	s_fpbmask;	/* fragments per block mask */
	__u32	s_apb;		/* address per block */
	__u32	s_2apb;		/* address per block^2 */
	__u32	s_3apb;		/* address per block^3 */
	__u32	s_apbmask;	/* address per block mask */
	__u32	s_apbshift;	/* address per block shift */
	__u32	s_2apbshift;	/* address per block shift * 2 */
	__u32	s_3apbshift;	/* address per block shift * 3 */
	__u32	s_nspfshift;	/* number of sector per fragment shift */
	__u32	s_nspb;		/* number of sector per block */
	__u32	s_inopf;	/* inodes per fragment */
	__u32	s_sbbase;	/* offset of NeXTstep superblock */
	__u32	s_bpf;		/* bits per fragment */
	__u32	s_bpfshift;	/* bits per fragment shift*/
	__u32	s_bpfmask;	/* bits per fragment mask */

	__u32	s_maxsymlinklen;/* upper limit on fast symlinks' size */
};


#define UFS_MAX_GROUP_LOADED 8
#define UFS_CGNO_EMPTY ((unsigned)-1)

struct ufs_sb_info {
	struct ufs_sb_private_info * s_uspi;	
	struct ufs_csum	* s_csp[UFS_MAXCSBUFS];
	unsigned s_bytesex;
	unsigned s_flags;
	struct buffer_head ** s_ucg;
	struct ufs_cg_private_info * s_ucpi[UFS_MAX_GROUP_LOADED]; 
	unsigned s_cgno[UFS_MAX_GROUP_LOADED];
	unsigned short s_cg_loaded;
	unsigned s_mount_opt;
};

/*
 * Sizes of this structures are:
 *	ufs_super_block_first	512
 *	ufs_super_block_second	512
 *	ufs_super_block_third	356
 */
struct ufs_super_block_first {
	__u32	fs_link;
	__u32	fs_rlink;
	__u32	fs_sblkno;
	__u32	fs_cblkno;
	__u32	fs_iblkno;
	__u32	fs_dblkno;
	__u32	fs_cgoffset;
	__u32	fs_cgmask;
	__u32	fs_time;
	__u32	fs_size;
	__u32	fs_dsize;
	__u32	fs_ncg;
	__u32	fs_bsize;
	__u32	fs_fsize;
	__u32	fs_frag;
	__u32	fs_minfree;
	__u32	fs_rotdelay;
	__u32	fs_rps;
	__u32	fs_bmask;
	__u32	fs_fmask;
	__u32	fs_bshift;
	__u32	fs_fshift;
	__u32	fs_maxcontig;
	__u32	fs_maxbpg;
	__u32	fs_fragshift;
	__u32	fs_fsbtodb;
	__u32	fs_sbsize;
	__u32	fs_csmask;
	__u32	fs_csshift;
	__u32	fs_nindir;
	__u32	fs_inopb;
	__u32	fs_nspf;
	__u32	fs_optim;
	union {
		struct {
			__u32	fs_npsect;
		} fs_sun;
		struct {
			__s32	fs_state;
		} fs_sunx86;
	} fs_u1;
	__u32	fs_interleave;
	__u32	fs_trackskew;
	__u32	fs_id[2];
	__u32	fs_csaddr;
	__u32	fs_cssize;
	__u32	fs_cgsize;
	__u32	fs_ntrak;
	__u32	fs_nsect;
	__u32	fs_spc;
	__u32	fs_ncyl;
	__u32	fs_cpg;
	__u32	fs_ipg;
	__u32	fs_fpg;
	struct ufs_csum fs_cstotal;
	__s8	fs_fmod;
	__s8	fs_clean;
	__s8	fs_ronly;
	__s8	fs_flags;
	__s8	fs_fsmnt[UFS_MAXMNTLEN - 212];

};

struct ufs_super_block_second {
	__s8	fs_fsmnt[212];
	__u32	fs_cgrotor;
	__u32	fs_csp[UFS_MAXCSBUFS];
	__u32	fs_maxcluster;
	__u32	fs_cpc;
	__u16	fs_opostbl[82];
};	

struct ufs_super_block_third {
	__u16	fs_opostbl[46];
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
	__s32	fs_postblformat;
	__s32	fs_nrpos;
	__s32	fs_postbloff;
	__s32	fs_rotbloff;
	__s32	fs_magic;
	__u8	fs_space[1];
};

#endif /* __LINUX_UFS_FS_SB_H */
