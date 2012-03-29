/*-
 * Copyright 2000 Hans Reiser
 * See README for licensing and copyright details
 * 
 * Ported to FreeBSD by Jean-Sébastien Pédron <jspedron@club-internet.fr>
 * 
 * $FreeBSD$
 */

#ifndef _GNU_REISERFS_REISERFS_FS_H
#define	_GNU_REISERFS_REISERFS_FS_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/endian.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/unistd.h>

#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/syslog.h>

#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/stat.h>
//#include <sys/mutex.h>

#include <sys/ctype.h>
#include <sys/bitstring.h>

#include <geom/geom.h>
#include <geom/geom_vfs.h>

#include <gnu/fs/reiserfs/reiserfs_mount.h>
#include <gnu/fs/reiserfs/reiserfs_fs_sb.h>
#include <gnu/fs/reiserfs/reiserfs_fs_i.h>

/* n must be power of 2 */
#define	_ROUND_UP(x, n)	(((x) + (n) - 1u) & ~((n) - 1u))

/* To be ok for alpha and others we have to align structures to 8 byte
 * boundary. */
#define	ROUND_UP(x)	_ROUND_UP(x, 8LL)

/* -------------------------------------------------------------------
 * Global variables
 * -------------------------------------------------------------------*/

extern struct vop_vector reiserfs_vnodeops;
extern struct vop_vector reiserfs_specops;

/* -------------------------------------------------------------------
 * Super block
 * -------------------------------------------------------------------*/

#define	REISERFS_BSIZE 1024

/* ReiserFS leaves the first 64k unused, so that partition labels have
 * enough space. If someone wants to write a fancy bootloader that needs
 * more than 64k, let us know, and this will be increased in size.
 * This number must be larger than than the largest block size on any
 * platform, or code will break. -Hans */
#define	REISERFS_DISK_OFFSET 64
#define	REISERFS_DISK_OFFSET_IN_BYTES                                        \
    ((REISERFS_DISK_OFFSET) * (REISERFS_BSIZE))

/* The spot for the super in versions 3.5 - 3.5.10 (inclusive) */
#define	REISERFS_OLD_DISK_OFFSET 8
#define	REISERFS_OLD_DISK_OFFSET_IN_BYTES                                    \
    ((REISERFS_OLD_DISK_OFFSET) * (REISERFS_BSIZE))

/*
 * Structure of a super block on disk, a version of which in RAM is
 * often accessed as REISERFS_SB(s)->r_rs. The version in RAM is part of
 * a larger structure containing fields never written to disk.
 */

#define	UNSET_HASH	0 /* read_super will guess about, what hash names
			     in directories were sorted with */
#define	TEA_HASH	1
#define	YURA_HASH	2
#define	R5_HASH		3
#define	DEFAULT_HASH	R5_HASH

struct journal_params {
	uint32_t	jp_journal_1st_block;      /* Where does journal start
						      from on its device */
	uint32_t	jp_journal_dev;            /* Journal device st_rdev */
	uint32_t	jp_journal_size;           /* Size of the journal */
	uint32_t	jp_journal_trans_max;      /* Max number of blocks in
						      a transaction */
	uint32_t	jp_journal_magic;          /* Random value made on
						      fs creation (this was
						      sb_journal_block_count) */
	uint32_t	jp_journal_max_batch;      /* Max number of blocks to
						      batch into a
						      transaction */
	uint32_t	jp_journal_max_commit_age; /* In seconds, how old can
						      an async commit be */
	uint32_t	jp_journal_max_trans_age;  /* In seconds, how old a
						      transaction be */
};

struct reiserfs_super_block_v1 {
	uint32_t	s_block_count; /* Blocks count      */
	uint32_t	s_free_blocks; /* Free blocks count */
	uint32_t	s_root_block;  /* Root block number */

	struct journal_params s_journal;

	uint16_t	s_blocksize;
	uint16_t	s_oid_maxsize;
	uint16_t	s_oid_cursize;
	uint16_t	s_umount_state;

	char 		s_magic[10];

	uint16_t	s_fs_state;
	uint32_t	s_hash_function_code;
	uint16_t	s_tree_height;
	uint16_t	s_bmap_nr;
	uint16_t	s_version;
	uint16_t	s_reserved_for_journal;
} __packed;

#define	SB_SIZE_V1 (sizeof(struct reiserfs_super_block_v1))

struct reiserfs_super_block {
	struct reiserfs_super_block_v1 s_v1;
	uint32_t	s_inode_generation;
	uint32_t	s_flags;
	unsigned char	s_uuid[16];
	unsigned char	s_label[16];
	char		s_unused[88];
} __packed;

#define	SB_SIZE (sizeof(struct reiserfs_super_block))

#define	REISERFS_VERSION_1	0
#define	REISERFS_VERSION_2	2

#define	REISERFS_SB(sbi)		(sbi)
#define	SB_DISK_SUPER_BLOCK(sbi)	(REISERFS_SB(sbi)->s_rs)
#define	SB_V1_DISK_SUPER_BLOCK(sbi)	(&(SB_DISK_SUPER_BLOCK(sbi)->s_v1))

#define	SB_BLOCKSIZE(sbi)						\
    le32toh((SB_V1_DISK_SUPER_BLOCK(sbi)->s_blocksize))
#define	SB_BLOCK_COUNT(sbi)						\
    le32toh((SB_V1_DISK_SUPER_BLOCK(sbi)->s_block_count))
#define	SB_FREE_BLOCKS(s)						\
    le32toh((SB_V1_DISK_SUPER_BLOCK(sbi)->s_free_blocks))

#define	SB_REISERFS_MAGIC(sbi)						\
    (SB_V1_DISK_SUPER_BLOCK(sbi)->s_magic)

#define	SB_ROOT_BLOCK(sbi)						\
    le32toh((SB_V1_DISK_SUPER_BLOCK(sbi)->s_root_block))

#define	SB_TREE_HEIGHT(sbi)						\
    le16toh((SB_V1_DISK_SUPER_BLOCK(sbi)->s_tree_height))

#define	SB_REISERFS_STATE(sbi)						\
    le16toh((SB_V1_DISK_SUPER_BLOCK(sbi)->s_umount_state))

#define	SB_VERSION(sbi)	le16toh((SB_V1_DISK_SUPER_BLOCK(sbi)->s_version))
#define	SB_BMAP_NR(sbi)	le16toh((SB_V1_DISK_SUPER_BLOCK(sbi)->s_bmap_nr))

#define	REISERFS_SUPER_MAGIC_STRING	"ReIsErFs"
#define	REISER2FS_SUPER_MAGIC_STRING	"ReIsEr2Fs"
#define	REISER2FS_JR_SUPER_MAGIC_STRING	"ReIsEr3Fs"

extern const char reiserfs_3_5_magic_string[];
extern const char reiserfs_3_6_magic_string[];
extern const char reiserfs_jr_magic_string[];

int	is_reiserfs_3_5(struct reiserfs_super_block *rs);
int	is_reiserfs_3_6(struct reiserfs_super_block *rs);
int	is_reiserfs_jr(struct reiserfs_super_block *rs);

/* ReiserFS internal error code (used by search_by_key and fix_nodes) */
#define	IO_ERROR	-2

typedef uint32_t b_blocknr_t;
typedef uint32_t unp_t;

struct unfm_nodeinfo {
	unp_t		unfm_nodenum;
	unsigned short	unfm_freespace;
};

/* There are two formats of keys: 3.5 and 3.6 */
#define	KEY_FORMAT_3_5	0
#define	KEY_FORMAT_3_6	1

/* There are two stat datas */
#define	STAT_DATA_V1	0
#define	STAT_DATA_V2	1

#define	REISERFS_I(ip)	(ip)

#define	get_inode_item_key_version(ip)					\
    ((REISERFS_I(ip)->i_flags & i_item_key_version_mask) ?		\
     KEY_FORMAT_3_6 : KEY_FORMAT_3_5)

#define	set_inode_item_key_version(ip, version) ({			\
	if ((version) == KEY_FORMAT_3_6)				\
    		REISERFS_I(ip)->i_flags |= i_item_key_version_mask;	\
    	else								\
    		REISERFS_I(ip)->i_flags &= ~i_item_key_version_mask;	\
})

#define	get_inode_sd_version(ip)					\
    ((REISERFS_I(ip)->i_flags & i_stat_data_version_mask) ?		\
     STAT_DATA_V2 : STAT_DATA_V1)

#define	set_inode_sd_version(inode, version) ({				\
	if((version) == STAT_DATA_V2)					\
		REISERFS_I(ip)->i_flags |= i_stat_data_version_mask;	\
	else								\
		REISERFS_I(ip)->i_flags &= ~i_stat_data_version_mask;	\
})

/* Values for s_umount_state field */
#define	REISERFS_VALID_FS	1
#define	REISERFS_ERROR_FS	2

/* There are 5 item types currently */
#define	TYPE_STAT_DATA		0
#define	TYPE_INDIRECT		1
#define	TYPE_DIRECT		2
#define	TYPE_DIRENTRY		3
#define	TYPE_MAXTYPE		3
#define	TYPE_ANY		15

/* -------------------------------------------------------------------
 * Key & item head
 * -------------------------------------------------------------------*/

struct offset_v1 {
	uint32_t	k_offset;
	uint32_t	k_uniqueness;
} __packed;

struct offset_v2 {
#if BYTE_ORDER == LITTLE_ENDIAN
	/* little endian version */
	uint64_t	k_offset:60;
	uint64_t	k_type:4;
#else
	/* big endian version */
	uint64_t	k_type:4;
	uint64_t	k_offset:60;
#endif
} __packed;

#if (BYTE_ORDER == BIG_ENDIAN)
typedef union {
	struct offset_v2	offset_v2;
	uint64_t		linear;
} __packed offset_v2_esafe_overlay;

static inline uint16_t
offset_v2_k_type(const struct offset_v2 *v2)
{

	offset_v2_esafe_overlay tmp = *(const offset_v2_esafe_overlay *)v2;
	tmp.linear = le64toh(tmp.linear);
	return ((tmp.offset_v2.k_type <= TYPE_MAXTYPE) ?
	    tmp.offset_v2.k_type : TYPE_ANY);
}

static inline void
set_offset_v2_k_type(struct offset_v2 *v2, int type)
{

	offset_v2_esafe_overlay *tmp = (offset_v2_esafe_overlay *)v2;
	tmp->linear = le64toh(tmp->linear);
	tmp->offset_v2.k_type = type;
	tmp->linear = htole64(tmp->linear);
}

static inline off_t
offset_v2_k_offset(const struct offset_v2 *v2)
{

	offset_v2_esafe_overlay tmp = *(const offset_v2_esafe_overlay *)v2;
	tmp.linear = le64toh(tmp.linear);
	return (tmp.offset_v2.k_offset);
}

static inline void
set_offset_v2_k_offset(struct offset_v2 *v2, off_t offset)
{

	offset_v2_esafe_overlay *tmp = (offset_v2_esafe_overlay *)v2;
	tmp->linear = le64toh(tmp->linear);
	tmp->offset_v2.k_offset = offset;
	tmp->linear = htole64(tmp->linear);
}
#else /* BYTE_ORDER != BIG_ENDIAN */
#define	offset_v2_k_type(v2)		((v2)->k_type)
#define	set_offset_v2_k_type(v2, val)	(offset_v2_k_type(v2) = (val))
#define	offset_v2_k_offset(v2)		((v2)->k_offset)
#define	set_offset_v2_k_offset(v2, val)	(offset_v2_k_offset(v2) = (val))
#endif /* BYTE_ORDER == BIG_ENDIAN */

/*
 * Key of an item determines its location in the S+tree, and
 * is composed of 4 components
 */
struct key {
	uint32_t	k_dir_id;    /* Packing locality: by default parent
					directory object id */
	uint32_t	k_objectid;  /* Object identifier */
	union {
		struct offset_v1	k_offset_v1;
		struct offset_v2	k_offset_v2;
	} __packed u;
} __packed;

struct cpu_key {
	struct key	on_disk_key;
	int		version;
	int		key_length; /* 3 in all cases but direct2indirect
				       and indirect2direct conversion */
};

/*
 * Our function for comparing keys can compare keys of different
 * lengths. It takes as a parameter the length of the keys it is to
 * compare. These defines are used in determining what is to be passed
 * to it as that parameter.
 */
#define	REISERFS_FULL_KEY_LEN	4
#define	REISERFS_SHORT_KEY_LEN	2

#define	KEY_SIZE	(sizeof(struct key))
#define	SHORT_KEY_SIZE	(sizeof(uint32_t) + sizeof(uint32_t))

/* Return values for search_by_key and clones */
#define	ITEM_FOUND		 1
#define	ITEM_NOT_FOUND		 0
#define	ENTRY_FOUND		 1
#define	ENTRY_NOT_FOUND		 0
#define	DIRECTORY_NOT_FOUND	-1
#define	REGULAR_FILE_FOUND	-2
#define	DIRECTORY_FOUND		-3
#define	BYTE_FOUND		 1
#define	BYTE_NOT_FOUND		 0
#define	FILE_NOT_FOUND		-1

#define	POSITION_FOUND		 1
#define	POSITION_NOT_FOUND	 0

/* Return values for reiserfs_find_entry and search_by_entry_key */
#define	NAME_FOUND		1
#define	NAME_NOT_FOUND		0
#define	GOTO_PREVIOUS_ITEM	2
#define	NAME_FOUND_INVISIBLE	3

/*
 * Everything in the filesystem is stored as a set of items. The item
 * head contains the key of the item, its free space (for indirect
 * items) and specifies the location of the item itself within the
 * block.
 */
struct item_head {
	/*
	 * Everything in the tree is found by searching for it based on
	 * its key.
	 */
	struct key	ih_key;
	union {
		/*
		 * The free space in the last unformatted node of an
		 * indirect item if this is an indirect item. This
		 * equals 0xFFFF iff this is a direct item or stat data
		 * item. Note that the key, not this field, is used to
		 * determine the item type, and thus which field this
		 * union contains.
		 */
		uint16_t	ih_free_space_reserved;

		/*
		 * If this is a directory item, this field equals the number of
		 * directory entries in the directory item.
		 */
		uint16_t	ih_entry_count;
	} __packed u;
	uint16_t	ih_item_len;      /* Total size of the item body */
	uint16_t	ih_item_location; /* An offset to the item body within
					     the block */
	uint16_t	ih_version;       /* 0 for all old items, 2 for new
					     ones. Highest bit is set by fsck
					     temporary, cleaned after all
					     done */
} __packed;

/* Size of item header */
#define	IH_SIZE (sizeof(struct item_head))

#define	ih_free_space(ih)	le16toh((ih)->u.ih_free_space_reserved)
#define	ih_version(ih)		le16toh((ih)->ih_version)
#define	ih_entry_count(ih)	le16toh((ih)->u.ih_entry_count)
#define	ih_location(ih)		le16toh((ih)->ih_item_location)
#define	ih_item_len(ih)		le16toh((ih)->ih_item_len)

/*
 * These operate on indirect items, where you've got an array of ints at
 * a possibly unaligned location. These are a noop on IA32.
 * 
 * p is the array of uint32_t, i is the index into the array, v is the
 * value to store there.
 */
#define	get_unaligned(ptr)						\
    ({ __typeof__(*(ptr)) __tmp;					\
     memcpy(&__tmp, (ptr), sizeof(*(ptr))); __tmp; })

#define	put_unaligned(val, ptr)						\
    ({ __typeof__(*(ptr)) __tmp = (val);				\
     memcpy((ptr), &__tmp, sizeof(*(ptr)));				\
     (void)0; })

#define	get_block_num(p, i)	le32toh(get_unaligned((p) + (i)))
#define	put_block_num(p, i, v)	put_unaligned(htole32(v), (p) + (i))

/* In old version uniqueness field shows key type */
#define	V1_SD_UNIQUENESS	0
#define	V1_INDIRECT_UNIQUENESS	0xfffffffe
#define	V1_DIRECT_UNIQUENESS	0xffffffff
#define	V1_DIRENTRY_UNIQUENESS	500
#define	V1_ANY_UNIQUENESS	555

/* Here are conversion routines */
static inline int	uniqueness2type(uint32_t uniqueness);
static inline uint32_t	type2uniqueness(int type);

static inline int
uniqueness2type(uint32_t uniqueness)
{

	switch ((int)uniqueness) {
	case V1_SD_UNIQUENESS:
		return (TYPE_STAT_DATA);
	case V1_INDIRECT_UNIQUENESS:
		return (TYPE_INDIRECT);
	case V1_DIRECT_UNIQUENESS:
		return (TYPE_DIRECT);
	case V1_DIRENTRY_UNIQUENESS:
		return (TYPE_DIRENTRY);
	default:
		log(LOG_NOTICE, "reiserfs: unknown uniqueness (%u)\n",
		    uniqueness);
	case V1_ANY_UNIQUENESS:
		return (TYPE_ANY);
	}
}

static inline uint32_t
type2uniqueness(int type)
{

	switch (type) {
	case TYPE_STAT_DATA:
		return (V1_SD_UNIQUENESS);
	case TYPE_INDIRECT:
		return (V1_INDIRECT_UNIQUENESS);
	case TYPE_DIRECT:
		return (V1_DIRECT_UNIQUENESS);
	case TYPE_DIRENTRY:
		return (V1_DIRENTRY_UNIQUENESS);
	default:
		log(LOG_NOTICE, "reiserfs: unknown type (%u)\n", type);
	case TYPE_ANY:
		return (V1_ANY_UNIQUENESS);
	}
}

/*
 * Key is pointer to on disk key which is stored in le, result is cpu,
 * there is no way to get version of object from key, so, provide
 * version to these defines.
 */
static inline off_t
le_key_k_offset(int version, const struct key *key)
{

	return ((version == KEY_FORMAT_3_5) ?
	    le32toh(key->u.k_offset_v1.k_offset) :
	    offset_v2_k_offset(&(key->u.k_offset_v2)));
}

static inline off_t
le_ih_k_offset(const struct item_head *ih)
{

	return (le_key_k_offset(ih_version(ih), &(ih->ih_key)));
}

static inline off_t
le_key_k_type(int version, const struct key *key)
{

	return ((version == KEY_FORMAT_3_5) ?
	    uniqueness2type(le32toh(key->u.k_offset_v1.k_uniqueness)) :
	    offset_v2_k_type(&(key->u.k_offset_v2)));
}

static inline off_t
le_ih_k_type(const struct item_head *ih)
{
	return (le_key_k_type(ih_version(ih), &(ih->ih_key)));
}

static inline void
set_le_key_k_offset(int version, struct key *key, off_t offset)
{

	(version == KEY_FORMAT_3_5) ?
	    (key->u.k_offset_v1.k_offset = htole32(offset)) :
	    (set_offset_v2_k_offset(&(key->u.k_offset_v2), offset));
}

static inline void
set_le_ih_k_offset(struct item_head *ih, off_t offset)
{

	set_le_key_k_offset(ih_version(ih), &(ih->ih_key), offset);
}

static inline void
set_le_key_k_type(int version, struct key *key, int type)
{

	(version == KEY_FORMAT_3_5) ?
	    (key->u.k_offset_v1.k_uniqueness =
	     htole32(type2uniqueness(type))) :
	    (set_offset_v2_k_type(&(key->u.k_offset_v2), type));
}

static inline void
set_le_ih_k_type(struct item_head *ih, int type)
{

	set_le_key_k_type(ih_version(ih), &(ih->ih_key), type);
}

#define	is_direntry_le_key(version, key)				\
    (le_key_k_type(version, key) == TYPE_DIRENTRY)
#define	is_direct_le_key(version, key)					\
    (le_key_k_type(version, key) == TYPE_DIRECT)
#define	is_indirect_le_key(version, key)				\
    (le_key_k_type(version, key) == TYPE_INDIRECT)
#define	is_statdata_le_key(version, key)				\
    (le_key_k_type(version, key) == TYPE_STAT_DATA)

/* Item header has version. */
#define	is_direntry_le_ih(ih)						\
    is_direntry_le_key(ih_version(ih), &((ih)->ih_key))
#define	is_direct_le_ih(ih)						\
    is_direct_le_key(ih_version(ih), &((ih)->ih_key))
#define	is_indirect_le_ih(ih)						\
    is_indirect_le_key(ih_version(ih), &((ih)->ih_key))
#define	is_statdata_le_ih(ih)						\
    is_statdata_le_key(ih_version(ih), &((ih)->ih_key))

static inline void
set_cpu_key_k_offset(struct cpu_key *key, off_t offset)
{

	(key->version == KEY_FORMAT_3_5) ?
	    (key->on_disk_key.u.k_offset_v1.k_offset = offset) :
	    (key->on_disk_key.u.k_offset_v2.k_offset = offset);
}

static inline void
set_cpu_key_k_type(struct cpu_key *key, int type)
{

	(key->version == KEY_FORMAT_3_5) ?
	    (key->on_disk_key.u.k_offset_v1.k_uniqueness =
	     type2uniqueness(type)):
	    (key->on_disk_key.u.k_offset_v2.k_type = type);
}

#define	is_direntry_cpu_key(key)	(cpu_key_k_type (key) == TYPE_DIRENTRY)
#define	is_direct_cpu_key(key)		(cpu_key_k_type (key) == TYPE_DIRECT)
#define	is_indirect_cpu_key(key)	(cpu_key_k_type (key) == TYPE_INDIRECT)
#define	is_statdata_cpu_key(key)	(cpu_key_k_type (key) == TYPE_STAT_DATA)

/* Maximal length of item */
#define	MAX_ITEM_LEN(block_size)	(block_size - BLKH_SIZE - IH_SIZE)
#define	MIN_ITEM_LEN			1

/* Object identifier for root dir */
#define	REISERFS_ROOT_OBJECTID		2
#define	REISERFS_ROOT_PARENT_OBJECTID	1

/* key is pointer to cpu key, result is cpu */
static inline off_t
cpu_key_k_offset(const struct cpu_key *key)
{

	return ((key->version == KEY_FORMAT_3_5) ?
	    key->on_disk_key.u.k_offset_v1.k_offset :
	    key->on_disk_key.u.k_offset_v2.k_offset);
}

static inline off_t
cpu_key_k_type(const struct cpu_key *key)
{

	return ((key->version == KEY_FORMAT_3_5) ?
	    uniqueness2type(key->on_disk_key.u.k_offset_v1.k_uniqueness) :
	    key->on_disk_key.u.k_offset_v2.k_type);
}

/*
 * Header of a disk block.  More precisely, header of a formatted leaf
 * or internal node, and not the header of an unformatted node.
 */
struct block_head {
	uint16_t	blk_level;            /* Level of a block in the
						 tree. */
	uint16_t	blk_nr_item;          /* Number of keys/items in a
						 block. */
	uint16_t	blk_free_space;       /* Block free space in bytes. */
	uint16_t	blk_reserved;         /* Dump this in v4/planA */
	struct key	blk_right_delim_key;  /* Kept only for compatibility */
};

#define	BLKH_SIZE		(sizeof(struct block_head))
#define	blkh_level(p_blkh)	(le16toh((p_blkh)->blk_level))
#define	blkh_nr_item(p_blkh)	(le16toh((p_blkh)->blk_nr_item))
#define	blkh_free_space(p_blkh)	(le16toh((p_blkh)->blk_free_space))

#define	FREE_LEVEL	0 /* When node gets removed from the tree its
			     blk_level is set to FREE_LEVEL. It is then
			     used to see whether the node is still in the
			     tree */

/* Values for blk_level field of the struct block_head */
#define	DISK_LEAF_NODE_LEVEL	1 /* Leaf node level.*/

/*
 * Given the buffer head of a formatted node, resolve to the block head
 * of that node.
 */
#define	B_BLK_HEAD(p_s_bp)	((struct block_head *)((p_s_bp)->b_data))
#define	B_NR_ITEMS(p_s_bp)	(blkh_nr_item(B_BLK_HEAD(p_s_bp)))
#define	B_LEVEL(p_s_bp)		(blkh_level(B_BLK_HEAD(p_s_bp)))
#define	B_FREE_SPACE(p_s_bp)	(blkh_free_space(B_BLK_HEAD(p_s_bp)))

/* -------------------------------------------------------------------
 * Stat data
 * -------------------------------------------------------------------*/

/*
 * Old stat data is 32 bytes long. We are going to distinguish new one
 * by different size.
 */
struct stat_data_v1 {
	uint16_t	sd_mode;  /* File type, permissions */
	uint16_t	sd_nlink; /* Number of hard links */
	uint16_t	sd_uid;   /* Owner */
	uint16_t	sd_gid;   /* Group */
	uint32_t	sd_size;  /* File size */
	uint32_t	sd_atime; /* Time of last access */
	uint32_t	sd_mtime; /* Time file was last modified  */
	uint32_t	sd_ctime; /* Time inode (stat data) was last changed
				     (except changes to sd_atime and
				     sd_mtime) */
	union {
		uint32_t 	sd_rdev;
		uint32_t	sd_blocks;  /* Number of blocks file uses */
	} __packed u;
	uint32_t	sd_first_direct_byte; /* First byte of file which is
						 stored in a direct item:
						 except that if it equals 1
						 it is a symlink and if it
						 equals ~(uint32_t)0 there
						 is no direct item. The
						 existence of this field
						 really grates on me. Let's
						 replace it with a macro based
						 on sd_size and our tail
						 suppression policy. Someday.
						 -Hans */
} __packed;

#define	SD_V1_SIZE			(sizeof(struct stat_data_v1))
#define	stat_data_v1(ih)		(ih_version (ih) == KEY_FORMAT_3_5)
#define	sd_v1_mode(sdp)			(le16toh((sdp)->sd_mode))
#define	set_sd_v1_mode(sdp, v)		((sdp)->sd_mode = htole16(v))
#define	sd_v1_nlink(sdp)		(le16toh((sdp)->sd_nlink))
#define	set_sd_v1_nlink(sdp, v)		((sdp)->sd_nlink = htole16(v))
#define	sd_v1_uid(sdp)			(le16toh((sdp)->sd_uid))
#define	set_sd_v1_uid(sdp, v)		((sdp)->sd_uid = htole16(v))
#define	sd_v1_gid(sdp)			(le16toh((sdp)->sd_gid))
#define	set_sd_v1_gid(sdp, v)		((sdp)->sd_gid = htole16(v))
#define	sd_v1_size(sdp)			(le32toh((sdp)->sd_size))
#define	set_sd_v1_size(sdp, v)		((sdp)->sd_size = htole32(v))
#define	sd_v1_atime(sdp)		(le32toh((sdp)->sd_atime))
#define	set_sd_v1_atime(sdp, v)		((sdp)->sd_atime = htole32(v))
#define	sd_v1_mtime(sdp)		(le32toh((sdp)->sd_mtime))
#define	set_sd_v1_mtime(sdp, v)		((sdp)->sd_mtime = htole32(v))
#define	sd_v1_ctime(sdp)		(le32toh((sdp)->sd_ctime))
#define	set_sd_v1_ctime(sdp, v)		((sdp)->sd_ctime = htole32(v))
#define	sd_v1_rdev(sdp)			(le32toh((sdp)->u.sd_rdev))
#define	set_sd_v1_rdev(sdp, v)		((sdp)->u.sd_rdev = htole32(v))
#define	sd_v1_blocks(sdp)		(le32toh((sdp)->u.sd_blocks))
#define	set_sd_v1_blocks(sdp, v)	((sdp)->u.sd_blocks = htole32(v))
#define	sd_v1_first_direct_byte(sdp)					\
    (le32toh((sdp)->sd_first_direct_byte))
#define	set_sd_v1_first_direct_byte(sdp, v)				\
    ((sdp)->sd_first_direct_byte = htole32(v))

/*
 * We want common flags to have the same values as in ext2,
 * so chattr(1) will work without problems
 */
#include <fs/ext2fs/ext2fs.h>
#include <fs/ext2fs/ext2_dinode.h>
#define	REISERFS_IMMUTABLE_FL	EXT2_IMMUTABLE
#define	REISERFS_APPEND_FL	EXT2_APPEND
#define	REISERFS_SYNC_FL	EXT2_SYNC
#define	REISERFS_NOATIME_FL	EXT2_NOATIME
#define	REISERFS_NODUMP_FL	EXT2_NODUMP
#define	REISERFS_SECRM_FL	EXT2_SECRM
#define	REISERFS_UNRM_FL	EXT2_UNRM
#define	REISERFS_COMPR_FL	EXT2_COMPR
#define	REISERFS_NOTAIL_FL	EXT2_NOTAIL_FL

/*
 * Stat Data on disk (reiserfs version of UFS disk inode minus the
 * address blocks)
 */
struct stat_data {
	uint16_t	sd_mode;  /* File type, permissions */
	uint16_t	sd_attrs; /* Persistent inode flags */
	uint32_t	sd_nlink; /* Number of hard links */
	uint64_t	sd_size;  /* File size */
	uint32_t	sd_uid;   /* Owner */
	uint32_t	sd_gid;   /* Group */
	uint32_t	sd_atime; /* Time of last access */
	uint32_t	sd_mtime; /* Time file was last modified  */
	uint32_t	sd_ctime; /* Time inode (stat data) was last changed
				     (except changes to sd_atime and
				     sd_mtime) */
	uint32_t	sd_blocks;
	union {
		uint32_t	sd_rdev;
		uint32_t	sd_generation;
		//uint32_t	sd_first_direct_byte; 
		/*
		 * First byte of file which is stored in a
		 * direct item: except that if it equals 1
		 * it is a symlink and if it equals
		 * ~(uint32_t)0 there is no direct item.  The
		 * existence of this field really grates
		 * on me. Let's replace it with a macro
		 * based on sd_size and our tail
		 * suppression policy?
		 */
	} __packed u;
} __packed;

/* This is 44 bytes long */
#define	SD_SIZE				(sizeof(struct stat_data))
#define	SD_V2_SIZE			SD_SIZE
#define	stat_data_v2(ih)		(ih_version (ih) == KEY_FORMAT_3_6)
#define	sd_v2_mode(sdp)			(le16toh((sdp)->sd_mode))
#define	set_sd_v2_mode(sdp, v)		((sdp)->sd_mode = htole16(v))
/* sd_reserved */
/* set_sd_reserved */
#define	sd_v2_nlink(sdp)		(le32toh((sdp)->sd_nlink))
#define	set_sd_v2_nlink(sdp, v)		((sdp)->sd_nlink = htole32(v))
#define	sd_v2_size(sdp)			(le64toh((sdp)->sd_size))
#define	set_sd_v2_size(sdp, v)		((sdp)->sd_size = cpu_to_le64(v))
#define	sd_v2_uid(sdp)			(le32toh((sdp)->sd_uid))
#define	set_sd_v2_uid(sdp, v)		((sdp)->sd_uid = htole32(v))
#define	sd_v2_gid(sdp)			(le32toh((sdp)->sd_gid))
#define	set_sd_v2_gid(sdp, v)		((sdp)->sd_gid = htole32(v))
#define	sd_v2_atime(sdp)		(le32toh((sdp)->sd_atime))
#define	set_sd_v2_atime(sdp, v)		((sdp)->sd_atime = htole32(v))
#define	sd_v2_mtime(sdp)		(le32toh((sdp)->sd_mtime))
#define	set_sd_v2_mtime(sdp, v)		((sdp)->sd_mtime = htole32(v))
#define	sd_v2_ctime(sdp)		(le32toh((sdp)->sd_ctime))
#define	set_sd_v2_ctime(sdp, v)		((sdp)->sd_ctime = htole32(v))
#define	sd_v2_blocks(sdp)		(le32toh((sdp)->sd_blocks))
#define	set_sd_v2_blocks(sdp, v)	((sdp)->sd_blocks = htole32(v))
#define	sd_v2_rdev(sdp)			(le32toh((sdp)->u.sd_rdev))
#define	set_sd_v2_rdev(sdp, v)		((sdp)->u.sd_rdev = htole32(v))
#define	sd_v2_generation(sdp)		(le32toh((sdp)->u.sd_generation))
#define	set_sd_v2_generation(sdp, v)	((sdp)->u.sd_generation = htole32(v))
#define	sd_v2_attrs(sdp)		(le16toh((sdp)->sd_attrs))
#define	set_sd_v2_attrs(sdp, v)		((sdp)->sd_attrs = htole16(v))

/* -------------------------------------------------------------------
 * Directory structure
 * -------------------------------------------------------------------*/

#define	SD_OFFSET		0
#define	SD_UNIQUENESS		0
#define	DOT_OFFSET		1
#define	DOT_DOT_OFFSET		2
#define	DIRENTRY_UNIQUENESS	500

#define	FIRST_ITEM_OFFSET	1

struct reiserfs_de_head {
	uint32_t	deh_offset;    /* Third component of the directory
					  entry key */
	uint32_t	deh_dir_id;    /* Objectid of the parent directory of
					  the object, that is referenced by
					  directory entry */
	uint32_t	deh_objectid;  /* Objectid of the object, that is
					  referenced by directory entry */
	uint16_t	deh_location;  /* Offset of name in the whole item */
	uint16_t	deh_state;     /* Whether 1) entry contains stat data
					  (for future), and 2) whether entry
					  is hidden (unlinked) */
} __packed;

#define	DEH_SIZE			sizeof(struct reiserfs_de_head)
#define	deh_offset(p_deh)		(le32toh((p_deh)->deh_offset))
#define	deh_dir_id(p_deh)		(le32toh((p_deh)->deh_dir_id))
#define	deh_objectid(p_deh)		(le32toh((p_deh)->deh_objectid))
#define	deh_location(p_deh)		(le16toh((p_deh)->deh_location))
#define	deh_state(p_deh)		(le16toh((p_deh)->deh_state))

#define	put_deh_offset(p_deh, v)	((p_deh)->deh_offset = htole32((v)))
#define	put_deh_dir_id(p_deh, v)	((p_deh)->deh_dir_id = htole32((v)))
#define	put_deh_objectid(p_deh, v)	((p_deh)->deh_objectid = htole32((v)))
#define	put_deh_location(p_deh, v)	((p_deh)->deh_location = htole16((v)))
#define	put_deh_state(p_deh, v)		((p_deh)->deh_state = htole16((v)))

/* Empty directory contains two entries "." and ".." and their headers */
#define	EMPTY_DIR_SIZE							\
    (DEH_SIZE * 2 + ROUND_UP(strlen(".")) + ROUND_UP(strlen("..")))

/* Old format directories have this size when empty */
#define	EMPTY_DIR_SIZE_V1	(DEH_SIZE * 2 + 3)

#define	DEH_Statdata	0 /* Not used now */
#define	DEH_Visible	2

/* Macro to map Linux' *_bit function to bitstring.h macros */
#define	set_bit(bit, name)		bit_set((bitstr_t *)name, bit)
#define	clear_bit(bit, name)		bit_clear((bitstr_t *)name, bit)
#define	test_bit(bit, name)		bit_test((bitstr_t *)name, bit)

#define	set_bit_unaligned(bit, name)	set_bit(bit, name)
#define	clear_bit_unaligned(bit, name)	clear_bit(bit, name)
#define	test_bit_unaligned(bit, name)	test_bit(bit, name)

#define	mark_de_with_sd(deh)						\
    set_bit_unaligned(DEH_Statdata, &((deh)->deh_state))
#define	mark_de_without_sd(deh)						\
    clear_bit_unaligned(DEH_Statdata, &((deh)->deh_state))
#define	mark_de_visible(deh)						\
    set_bit_unaligned (DEH_Visible, &((deh)->deh_state))
#define	mark_de_hidden(deh)						\
    clear_bit_unaligned (DEH_Visible, &((deh)->deh_state))

#define	de_with_sd(deh)							\
    test_bit_unaligned(DEH_Statdata, &((deh)->deh_state))
#define	de_visible(deh)							\
    test_bit_unaligned(DEH_Visible, &((deh)->deh_state))
#define	de_hidden(deh)							\
    !test_bit_unaligned(DEH_Visible, &((deh)->deh_state))

/* Two entries per block (at least) */
#define	REISERFS_MAX_NAME(block_size)	255

/*
 * This structure is used for operations on directory entries. It is not
 * a disk structure. When reiserfs_find_entry or search_by_entry_key
 * find directory entry, they return filled reiserfs_dir_entry structure
 */
struct reiserfs_dir_entry {
	struct buf *de_bp;
	int	 de_item_num;
	struct item_head *de_ih;
	int	 de_entry_num;
	struct reiserfs_de_head *de_deh;
	int	 de_entrylen;
	int	 de_namelen;
	char	*de_name;
	char	*de_gen_number_bit_string;

	uint32_t de_dir_id;
	uint32_t de_objectid;

	struct cpu_key de_entry_key;
};

/* Pointer to file name, stored in entry */
#define	B_I_DEH_ENTRY_FILE_NAME(bp, ih, deh)				\
    (B_I_PITEM(bp, ih) + deh_location(deh))

/* Length of name */
#define	I_DEH_N_ENTRY_FILE_NAME_LENGTH(ih, deh, entry_num)		\
    (I_DEH_N_ENTRY_LENGTH(ih, deh, entry_num) -				\
     (de_with_sd(deh) ? SD_SIZE : 0))

/* Hash value occupies bits from 7 up to 30 */
#define	GET_HASH_VALUE(offset)		((offset) & 0x7fffff80LL)

/* Generation number occupies 7 bits starting from 0 up to 6 */
#define	GET_GENERATION_NUMBER(offset)	((offset) & 0x7fLL)
#define	MAX_GENERATION_NUMBER		127

/* Get item body */
#define	B_I_PITEM(bp, ih)	((bp)->b_data + ih_location(ih))
#define	B_I_DEH(bp, ih)		((struct reiserfs_de_head *)(B_I_PITEM(bp, ih)))

/*
 * Length of the directory entry in directory item. This define
 * calculates length of i-th directory entry using directory entry
 * locations from dir entry head. When it calculates length of 0-th
 * directory entry, it uses length of whole item in place of entry
 * location of the non-existent following entry in the calculation. See
 * picture above.
 */
static inline int
entry_length (const struct buf *bp, const struct item_head *ih,
    int pos_in_item)
{
	struct reiserfs_de_head *deh;

	deh = B_I_DEH(bp, ih) + pos_in_item;
	if (pos_in_item)
		return (deh_location(deh - 1) - deh_location(deh));

	return (ih_item_len(ih) - deh_location(deh));
}

/*
 * Number of entries in the directory item, depends on ENTRY_COUNT
 * being at the start of directory dynamic data.
 */
#define	I_ENTRY_COUNT(ih)	(ih_entry_count((ih)))

/* -------------------------------------------------------------------
 * Disk child
 * -------------------------------------------------------------------*/

/*
 * Disk child pointer: The pointer from an internal node of the tree
 * to a node that is on disk.
 */
struct disk_child {
	uint32_t	dc_block_number; /* Disk child's block number. */
	uint16_t	dc_size;         /* Disk child's used space. */
	uint16_t	dc_reserved;
};

#define	DC_SIZE			(sizeof(struct disk_child))
#define	dc_block_number(dc_p)	(le32toh((dc_p)->dc_block_number))
#define	dc_size(dc_p)		(le16toh((dc_p)->dc_size))
#define	put_dc_block_number(dc_p, val)					\
    do { (dc_p)->dc_block_number = htole32(val); } while (0)
#define	put_dc_size(dc_p, val)						\
    do { (dc_p)->dc_size = htole16(val); } while (0)

/* Get disk child by buffer header and position in the tree node. */
#define	B_N_CHILD(p_s_bp, n_pos)					\
    ((struct disk_child *)((p_s_bp)->b_data + BLKH_SIZE +		\
			   B_NR_ITEMS(p_s_bp) * KEY_SIZE +		\
			   DC_SIZE * (n_pos)))

/* Get disk child number by buffer header and position in the tree node. */
#define	B_N_CHILD_NUM(p_s_bp, n_pos)					\
    (dc_block_number(B_N_CHILD(p_s_bp, n_pos)))
#define	PUT_B_N_CHILD_NUM(p_s_bp, n_pos, val)				\
    (put_dc_block_number(B_N_CHILD(p_s_bp, n_pos), val))

/* -------------------------------------------------------------------
 * Path structures and defines
 * -------------------------------------------------------------------*/

struct path_element {
	struct buf	*pe_buffer;  /* Pointer to the buffer at the path in
					the tree. */
	int		pe_position; /* Position in the tree node which is
					placed in the buffer above. */
};

#define	MAX_HEIGHT			5 /* Maximal height of a tree. Don't
					     change this without changing
					     JOURNAL_PER_BALANCE_CNT */
#define	EXTENDED_MAX_HEIGHT		7 /* Must be equals MAX_HEIGHT +
					     FIRST_PATH_ELEMENT_OFFSET */
#define	FIRST_PATH_ELEMENT_OFFSET	2 /* Must be equal to at least 2. */
#define	ILLEGAL_PATH_ELEMENT_OFFSET	1 /* Must be equal to
					     FIRST_PATH_ELEMENT_OFFSET - 1 */
#define	MAX_FEB_SIZE			6 /* This MUST be MAX_HEIGHT + 1.
					     See about FEB below */

struct path {
	/* Length of the array below. */
	int	path_length;
	/* Array of the path element */
	struct path_element path_elements[EXTENDED_MAX_HEIGHT];
	int	pos_in_item;
};

#define	pos_in_item(path)	((path)->pos_in_item)

#ifdef __amd64__
/* To workaround a bug in gcc. He generates a call to memset() which
 * is a inline function; this causes a compile time error. */
#define	INITIALIZE_PATH(var)						\
    struct path var;							\
    bzero(&var, sizeof(var));						\
    var.path_length = ILLEGAL_PATH_ELEMENT_OFFSET;
#else
#define	INITIALIZE_PATH(var)						\
    struct path var = { ILLEGAL_PATH_ELEMENT_OFFSET, }
#endif

/* Get path element by path and path position. */
#define	PATH_OFFSET_PELEMENT(p_s_path, n_offset)			\
    ((p_s_path)->path_elements + (n_offset))

/* Get buffer header at the path by path and path position. */
#define	PATH_OFFSET_PBUFFER(p_s_path, n_offset)				\
    (PATH_OFFSET_PELEMENT(p_s_path, n_offset)->pe_buffer)

/* Get position in the element at the path by path and path position. */
#define	PATH_OFFSET_POSITION(p_s_path, n_offset)			\
    (PATH_OFFSET_PELEMENT(p_s_path, n_offset)->pe_position)

#define	PATH_PLAST_BUFFER(p_s_path)					\
    (PATH_OFFSET_PBUFFER((p_s_path), (p_s_path)->path_length))

#define	PATH_LAST_POSITION(p_s_path)					\
    (PATH_OFFSET_POSITION((p_s_path), (p_s_path)->path_length))

#define	PATH_PITEM_HEAD(p_s_path)					\
    B_N_PITEM_HEAD(PATH_PLAST_BUFFER(p_s_path), PATH_LAST_POSITION(p_s_path))

#define	get_last_bp(path)	PATH_PLAST_BUFFER(path)
#define	get_ih(path)		PATH_PITEM_HEAD(path)

/* -------------------------------------------------------------------
 * Misc.
 * -------------------------------------------------------------------*/

/* Size of pointer to the unformatted node. */
#define	UNFM_P_SIZE	(sizeof(unp_t))
#define	UNFM_P_SHIFT	2

/* In in-core inode key is stored on le form */
#define	INODE_PKEY(ip)	((struct key *)(REISERFS_I(ip)->i_key))

#define	MAX_UL_INT	0xffffffff
#define	MAX_INT		0x7ffffff
#define	MAX_US_INT	0xffff

/* The purpose is to detect overflow of an unsigned short */
#define	REISERFS_LINK_MAX	(MAX_US_INT - 1000)

#define	fs_generation(sbi)	(REISERFS_SB(sbi)->s_generation_counter)
#define	get_generation(sbi)	(fs_generation(sbi))

#define	__fs_changed(gen, sbi)	(gen != get_generation (sbi))
/*#define	fs_changed(gen, sbi)	({ cond_resched();		\
    __fs_changed(gen, sbi); })*/
#define	fs_changed(gen, sbi)	(__fs_changed(gen, sbi))

/* -------------------------------------------------------------------
 * Fixate node
 * -------------------------------------------------------------------*/

/*
 * To make any changes in the tree we always first find node, that
 * contains item to be changed/deleted or place to insert a new item.
 * We call this node S. To do balancing we need to decide what we will
 * shift to left/right neighbor, or to a new node, where new item will
 * be etc. To make this analysis simpler we build virtual node. Virtual
 * node is an array of items, that will replace items of node S. (For
 * instance if we are going to delete an item, virtual node does not
 * contain it). Virtual node keeps information about item sizes and
 * types, mergeability of first and last items, sizes of all entries in
 * directory item. We use this array of items when calculating what we
 * can shift to neighbors and how many nodes we have to have if we do
 * not any shiftings, if we shift to left/right neighbor or to both.
 */
struct virtual_item {
	int			 vi_index;    /* Index in the array of item
						 operations */
	unsigned short		 vi_type;     /* Left/right mergeability */
	unsigned short		 vi_item_len; /* Length of item that it will
						 have after balancing */
	struct item_head	*vi_ih;
	const char		*vi_item;     /* Body of item (old or new) */
	const void		*vi_new_data; /* 0 always but paste mode */
	void			*vi_uarea;    /* Item specific area */
};

struct virtual_node {
	char		*vn_free_ptr; /* This is a pointer to the free space
					 in the buffer */
	unsigned short	 vn_nr_item;  /* Number of items in virtual node */
	short		 vn_size;     /* Size of node , that node would have
					 if it has unlimited size and no
					 balancing is performed */
	short		 vn_mode;     /* Mode of balancing (paste, insert,
					 delete, cut) */
	short		 vn_affected_item_num;
	short		 vn_pos_in_item;
	struct item_head *vn_ins_ih;  /* Item header of inserted item, 0 for
					 other modes */
	const void	*vn_data;
	struct virtual_item *vn_vi;   /* Array of items (including a new one,
					 excluding item to be deleted) */
};

/* Used by directory items when creating virtual nodes */
struct direntry_uarea {
	int		flags;
	uint16_t	entry_count;
	uint16_t	entry_sizes[1];
} __packed;

/* -------------------------------------------------------------------
 * Tree balance
 * -------------------------------------------------------------------*/

struct reiserfs_iget_args {
	uint32_t	objectid;
	uint32_t	dirid;
};

struct item_operations {
	int	(*bytes_number)(struct item_head * ih, int block_size);
	void	(*decrement_key)(struct cpu_key *);
	int	(*is_left_mergeable)(struct key * ih, unsigned long bsize);
	void	(*print_item)(struct item_head *, char * item);
	void	(*check_item)(struct item_head *, char * item);

	int	(*create_vi)(struct virtual_node * vn,
	    struct virtual_item * vi, int is_affected, int insert_size);
	int	(*check_left)(struct virtual_item * vi, int free,
	    int start_skip, int end_skip);
	int	(*check_right)(struct virtual_item * vi, int free);
	int	(*part_size)(struct virtual_item * vi, int from, int to);
	int	(*unit_num)(struct virtual_item * vi);
	void	(*print_vi)(struct virtual_item * vi);
};

extern struct item_operations *item_ops[TYPE_ANY + 1];

#define	op_bytes_number(ih, bsize)					\
    item_ops[le_ih_k_type(ih)]->bytes_number(ih, bsize)

#define	COMP_KEYS	comp_keys
#define	COMP_SHORT_KEYS	comp_short_keys

/* Get the item header */
#define	B_N_PITEM_HEAD(bp, item_num)					\
    ((struct item_head *)((bp)->b_data + BLKH_SIZE) + (item_num))

/* Get key */
#define	B_N_PDELIM_KEY(bp, item_num)					\
    ((struct key *)((bp)->b_data + BLKH_SIZE) + (item_num))

/* -------------------------------------------------------------------
 * Function declarations
 * -------------------------------------------------------------------*/

/* reiserfs_stree.c */
int	B_IS_IN_TREE(const struct buf *p_s_bp);

extern void	copy_item_head(struct item_head * p_v_to,
		    const struct item_head * p_v_from);

extern int	comp_keys(const struct key *le_key,
		    const struct cpu_key *cpu_key);
extern int	comp_short_keys(const struct key *le_key,
		    const struct cpu_key *cpu_key);

extern int	comp_le_keys(const struct key *, const struct key *);

static inline int
le_key_version(const struct key *key)
{
	int type;

	type = offset_v2_k_type(&(key->u.k_offset_v2));
	if (type != TYPE_DIRECT && type != TYPE_INDIRECT &&
	    type != TYPE_DIRENTRY)
		return (KEY_FORMAT_3_5);

	return (KEY_FORMAT_3_6);
}

static inline void
copy_key(struct key *to, const struct key *from)
{

	memcpy(to, from, KEY_SIZE);
}

const struct key	*get_lkey(const struct path *p_s_chk_path,
			    const struct reiserfs_sb_info *p_s_sbi);
const struct key	*get_rkey(const struct path *p_s_chk_path,
			    const struct reiserfs_sb_info *p_s_sbi);
int	bin_search(const void * p_v_key, const void * p_v_base,
		    int p_n_num, int p_n_width, int * p_n_pos);

void	pathrelse(struct path *p_s_search_path);
int	reiserfs_check_path(struct path *p);

int	search_by_key(struct reiserfs_sb_info *p_s_sbi,
	    const struct cpu_key *p_s_key,
	    struct path *p_s_search_path,
	    int n_stop_level);
#define	search_item(sbi, key, path)					\
    search_by_key(sbi, key, path, DISK_LEAF_NODE_LEVEL)
int	search_for_position_by_key(struct reiserfs_sb_info *p_s_sbi,
	    const struct cpu_key *p_s_cpu_key,
	    struct path *p_s_search_path);
void	decrement_counters_in_path(struct path *p_s_search_path);

/* reiserfs_inode.c */
vop_read_t	reiserfs_read;
vop_inactive_t	reiserfs_inactive;
vop_reclaim_t	reiserfs_reclaim;

int	reiserfs_get_block(struct reiserfs_node *ip, long block,
	    off_t offset, struct uio *uio);

void	make_cpu_key(struct cpu_key *cpu_key, struct reiserfs_node *ip,
	    off_t offset, int type, int key_length);

void	reiserfs_read_locked_inode(struct reiserfs_node *ip,
	    struct reiserfs_iget_args *args);
int	reiserfs_iget(struct mount *mp, const struct cpu_key *key,
	    struct vnode **vpp, struct thread *td);

void	sd_attrs_to_i_attrs(uint16_t sd_attrs, struct reiserfs_node *ip);
void	i_attrs_to_sd_attrs(struct reiserfs_node *ip, uint16_t *sd_attrs);

/* reiserfs_namei.c */
vop_readdir_t		reiserfs_readdir;
vop_cachedlookup_t	reiserfs_lookup;

void	set_de_name_and_namelen(struct reiserfs_dir_entry * de);
int	search_by_entry_key(struct reiserfs_sb_info *sbi,
	    const struct cpu_key *key, struct path *path,
	    struct reiserfs_dir_entry *de);

/* reiserfs_prints.c */
char	*reiserfs_hashname(int code);
void	 reiserfs_dump_buffer(caddr_t buf, off_t len);

#if defined(REISERFS_DEBUG)
#define	reiserfs_log(lvl, fmt, ...)					\
    log(lvl, "ReiserFS/%s: " fmt, __func__, ## __VA_ARGS__)
#elif defined (REISERFS_DEBUG_CONS)
#define	reiserfs_log(lvl, fmt, ...)					\
    printf("%s:%d: " fmt, __func__, __LINE__, ## __VA_ARGS__)
#else
#define	reiserfs_log(lvl, fmt, ...)
#endif

#define	reiserfs_log_0(lvl, fmt, ...)					\
    printf("%s:%d: " fmt, __func__, __LINE__, ## __VA_ARGS__)

/* reiserfs_hashes.c */
uint32_t	keyed_hash(const signed char *msg, int len);
uint32_t	yura_hash(const signed char *msg, int len);
uint32_t	r5_hash(const signed char *msg, int len);

#define	reiserfs_test_le_bit  test_bit

#endif /* !defined _GNU_REISERFS_REISERFS_FS_H */
