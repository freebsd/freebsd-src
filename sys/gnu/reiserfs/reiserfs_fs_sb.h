/*
 * Copyright 2000 Hans Reiser
 * See README for licensing and copyright details
 * 
 * Ported to FreeBSD by Jean-Sébastien Pédron <jspedron@club-internet.fr>
 * 
 * $FreeBSD$
 */

#ifndef _GNU_REISERFS_REISERFS_FS_SB_H
#define _GNU_REISERFS_REISERFS_FS_SB_H

typedef uint32_t (*hashf_t)(const signed char *, int);

#define sb_block_count(sbp)		(le32toh((sbp)->s_v1.s_block_count))
#define set_sb_block_count(sbp,v)	((sbp)->s_v1.s_block_count = htole32(v))
#define sb_free_blocks(sbp)		(le32toh((sbp)->s_v1.s_free_blocks))
#define set_sb_free_blocks(sbp,v)	((sbp)->s_v1.s_free_blocks = htole32(v))
#define sb_root_block(sbp)		(le32toh((sbp)->s_v1.s_root_block))

/* Bitmaps */
struct reiserfs_bitmap_info {
	uint16_t	 first_zero_hint;
	uint16_t	 free_count;
	//struct buf	*bp;      /* The actual bitmap */
	caddr_t		 bp_data; /* The actual bitmap */
};

/* ReiserFS union of in-core super block data */
struct reiserfs_sb_info {
	struct reiserfs_super_block *s_rs;
	struct reiserfs_bitmap_info *s_ap_bitmap;
	struct vnode	*s_devvp;

	unsigned short	 s_mount_state;

	hashf_t		 s_hash_function;      /* Pointer to function which
						  is used to sort names in
						  directory. Set on mount */
	unsigned long	 s_mount_opt;          /* ReiserFS's mount options
						  are set here */
	int		 s_generation_counter; /* Increased by one every
						  time the tree gets
						  re-balanced */
	unsigned long	 s_properties;         /* File system properties.
						  Currently holds on-disk
						  FS format */
	uint16_t	 s_blocksize;
	uint16_t	 s_blocksize_bits;
	char		 s_rd_only;            /* Is it read-only ? */
	int		 s_is_unlinked_ok;
};

#define sb_version(sbi)			(le16toh((sbi)->s_v1.s_version))
#define set_sb_version(sbi, v)		((sbi)->s_v1.s_version = htole16(v))

#define sb_blocksize(sbi)		(le16toh((sbi)->s_v1.s_blocksize))
#define set_sb_blocksize(sbi, v)	((sbi)->s_v1.s_blocksize = htole16(v))

#define sb_hash_function_code(sbi)					\
    (le32toh((sbi)->s_v1.s_hash_function_code))
#define set_sb_hash_function_code(sbi, v)				\
    ((sbi)->s_v1.s_hash_function_code = htole32(v))

#define sb_bmap_nr(sbi)		(le16toh((sbi)->s_v1.s_bmap_nr))
#define set_sb_bmap_nr(sbi, v)	((sbi)->s_v1.s_bmap_nr = htole16(v))

/* Definitions of reiserfs on-disk properties: */
#define REISERFS_3_5	0
#define REISERFS_3_6	1

enum reiserfs_mount_options {
	/* Mount options */
	REISERFS_LARGETAIL,  /* Large tails will be created in a session */
	REISERFS_SMALLTAIL,  /* Small (for files less than block size) tails
				will be created in a session */
	REPLAYONLY,          /* Replay journal and return 0. Use by fsck */
	REISERFS_CONVERT,    /* -o conv: causes conversion of old format super
				block to the new format. If not specified -
				old partition will be dealt with in a manner
				of 3.5.x */

	/*
	 * -o hash={tea, rupasov, r5, detect} is meant for properly mounting
	 * reiserfs disks from 3.5.19 or earlier. 99% of the time, this option
	 * is not required. If the normal autodection code can't determine
	 * which hash to use (because both hases had the same value for a
	 * file) use this option to force a specific hash. It won't allow you
	 * to override the existing hash on the FS, so if you have a tea hash
	 * disk, and mount with -o hash=rupasov, the mount will fail.
	 */
	FORCE_TEA_HASH,      /* try to force tea hash on mount */
	FORCE_RUPASOV_HASH,  /* try to force rupasov hash on mount */
	FORCE_R5_HASH,       /* try to force rupasov hash on mount */
	FORCE_HASH_DETECT,   /* try to detect hash function on mount */

	REISERFS_DATA_LOG,
	REISERFS_DATA_ORDERED,
	REISERFS_DATA_WRITEBACK,

	/*
	 * used for testing experimental features, makes benchmarking new
	 * features with and without more convenient, should never be used by
	 * users in any code shipped to users (ideally)
	 */
	
	REISERFS_NO_BORDER,
	REISERFS_NO_UNHASHED_RELOCATION,
	REISERFS_HASHED_RELOCATION,
	REISERFS_ATTRS,
	REISERFS_XATTRS,
	REISERFS_XATTRS_USER,
	REISERFS_POSIXACL,

	REISERFS_TEST1,
	REISERFS_TEST2,
	REISERFS_TEST3,
	REISERFS_TEST4,
};

#define reiserfs_r5_hash(sbi)						\
    (REISERFS_SB(sbi)->s_mount_opt & (1 << FORCE_R5_HASH))
#define reiserfs_rupasov_hash(sbi)					\
    (REISERFS_SB(sbi)->s_mount_opt & (1 << FORCE_RUPASOV_HASH))
#define reiserfs_tea_hash(sbi)						\
    (REISERFS_SB(sbi)->s_mount_opt & (1 << FORCE_TEA_HASH))
#define reiserfs_hash_detect(sbi)					\
    (REISERFS_SB(sbi)->s_mount_opt & (1 << FORCE_HASH_DETECT))

#define reiserfs_attrs(sbi)						\
    (REISERFS_SB(sbi)->s_mount_opt & (1 << REISERFS_ATTRS))

#define reiserfs_data_log(sbi)						\
    (REISERFS_SB(sbi)->s_mount_opt & (1 << REISERFS_DATA_LOG))
#define reiserfs_data_ordered(sbi)					\
    (REISERFS_SB(sbi)->s_mount_opt & (1 << REISERFS_DATA_ORDERED))
#define reiserfs_data_writeback(sbi)					\
    (REISERFS_SB(sbi)->s_mount_opt & (1 << REISERFS_DATA_WRITEBACK))

#define SB_BUFFER_WITH_SB(sbi)	(REISERFS_SB(sbi)->s_sbh)
#define SB_AP_BITMAP(sbi)	(REISERFS_SB(sbi)->s_ap_bitmap)

#endif /* !defined _GNU_REISERFS_REISERFS_FS_SB_H */
