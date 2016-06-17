#ifndef _SYSV_FS_SB
#define _SYSV_FS_SB

/*
 * SystemV/V7/Coherent super-block data in memory
 * The SystemV/V7/Coherent superblock contains dynamic data (it gets modified
 * while the system is running). This is in contrast to the Minix and Berkeley
 * filesystems (where the superblock is never modified). This affects the
 * sync() operation: we must keep the superblock in a disk buffer and use this
 * one as our "working copy".
 */

struct sysv_sb_info {
	int	       s_type;		/* file system type: FSTYPE_{XENIX|SYSV|COH} */
	char	       s_bytesex;	/* bytesex (le/be/pdp) */
	char	       s_truncate;	/* if 1: names > SYSV_NAMELEN chars are truncated */
					/* if 0: they are disallowed (ENAMETOOLONG) */
	nlink_t        s_link_max;	/* max number of hard links to a file */
	unsigned int   s_inodes_per_block;	/* number of inodes per block */
	unsigned int   s_inodes_per_block_1;	/* inodes_per_block - 1 */
	unsigned int   s_inodes_per_block_bits;	/* log2(inodes_per_block) */
	unsigned int   s_ind_per_block;		/* number of indirections per block */
	unsigned int   s_ind_per_block_bits;	/* log2(ind_per_block) */
	unsigned int   s_ind_per_block_2;	/* ind_per_block ^ 2 */
	unsigned int   s_toobig_block;		/* 10 + ipb + ipb^2 + ipb^3 */
	unsigned int   s_block_base;	/* physical block number of block 0 */
	unsigned short s_fic_size;	/* free inode cache size, NICINOD */
	unsigned short s_flc_size;	/* free block list chunk size, NICFREE */
	/* The superblock is kept in one or two disk buffers: */
	struct buffer_head *s_bh1;
	struct buffer_head *s_bh2;
	/* These are pointers into the disk buffer, to compensate for
	   different superblock layout. */
	char *         s_sbd1;		/* entire superblock data, for part 1 */
	char *         s_sbd2;		/* entire superblock data, for part 2 */
	u16            *s_sb_fic_count;	/* pointer to s_sbd->s_ninode */
        u16            *s_sb_fic_inodes; /* pointer to s_sbd->s_inode */
	u16            *s_sb_total_free_inodes; /* pointer to s_sbd->s_tinode */
	u16            *s_bcache_count;	/* pointer to s_sbd->s_nfree */
	u32	       *s_bcache;	/* pointer to s_sbd->s_free */
	u32            *s_free_blocks;	/* pointer to s_sbd->s_tfree */
	u32            *s_sb_time;	/* pointer to s_sbd->s_time */
	u32            *s_sb_state;	/* pointer to s_sbd->s_state, only FSTYPE_SYSV */
	/* We keep those superblock entities that don't change here;
	   this saves us an indirection and perhaps a conversion. */
	u32            s_firstinodezone; /* index of first inode zone */
	u32            s_firstdatazone;	/* same as s_sbd->s_isize */
	u32            s_ninodes;	/* total number of inodes */
	u32            s_ndatazones;	/* total number of data zones */
	u32            s_nzones;	/* same as s_sbd->s_fsize */
	u16	       s_namelen;       /* max length of dir entry */
};
/* The field s_toobig_block is currently unused. */

/* sv_ == u.sysv_sb.s_ */
#define sv_type					u.sysv_sb.s_type
#define sv_bytesex				u.sysv_sb.s_bytesex
#define sv_truncate				u.sysv_sb.s_truncate
#define sv_link_max				u.sysv_sb.s_link_max
#define sv_inodes_per_block			u.sysv_sb.s_inodes_per_block
#define sv_inodes_per_block_1			u.sysv_sb.s_inodes_per_block_1
#define sv_inodes_per_block_bits		u.sysv_sb.s_inodes_per_block_bits
#define sv_ind_per_block			u.sysv_sb.s_ind_per_block
#define sv_ind_per_block_bits			u.sysv_sb.s_ind_per_block_bits
#define sv_ind_per_block_2			u.sysv_sb.s_ind_per_block_2
#define sv_toobig_block				u.sysv_sb.s_toobig_block
#define sv_block_base				u.sysv_sb.s_block_base
#define sv_fic_size				u.sysv_sb.s_fic_size
#define sv_flc_size				u.sysv_sb.s_flc_size
#define sv_bh1					u.sysv_sb.s_bh1
#define sv_bh2					u.sysv_sb.s_bh2
#define sv_sbd1					u.sysv_sb.s_sbd1
#define sv_sbd2					u.sysv_sb.s_sbd2
#define sv_sb_fic_count				u.sysv_sb.s_sb_fic_count
#define sv_sb_fic_inodes			u.sysv_sb.s_sb_fic_inodes
#define sv_sb_total_free_inodes			u.sysv_sb.s_sb_total_free_inodes
#define sv_bcache_count				u.sysv_sb.s_bcache_count
#define sv_bcache				u.sysv_sb.s_bcache
#define sv_free_blocks				u.sysv_sb.s_free_blocks
#define sv_sb_time				u.sysv_sb.s_sb_time
#define sv_sb_state				u.sysv_sb.s_sb_state
#define sv_firstinodezone			u.sysv_sb.s_firstinodezone
#define sv_firstdatazone			u.sysv_sb.s_firstdatazone
#define sv_ninodes				u.sysv_sb.s_ninodes
#define sv_ndatazones				u.sysv_sb.s_ndatazones
#define sv_nzones				u.sysv_sb.s_nzones
#define sv_namelen                              u.sysv_sb.s_namelen

#endif
