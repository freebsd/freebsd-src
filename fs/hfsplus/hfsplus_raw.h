/*
 *  linux/include/linux/hfsplus_raw.h
 *
 * Copyright (C) 1999
 * Brad Boyer (flar@pants.nu)
 * (C) 2003 Ardis Technologies <roman@ardistech.com>
 *
 * Format of structures on disk
 * Information taken from Apple Technote #1150 (HFS Plus Volume Format)
 *
 */

#ifndef _LINUX_HFSPLUS_RAW_H
#define _LINUX_HFSPLUS_RAW_H

#include <linux/types.h>

#define __packed __attribute__ ((packed))

/* Some constants */
#define HFSPLUS_SECTOR_SIZE        512
#define HFSPLUS_VOLHEAD_SECTOR       2
#define HFSPLUS_VOLHEAD_SIG     0x482b
#define HFSPLUS_SUPER_MAGIC     0x482b
#define HFSPLUS_CURRENT_VERSION      4

#define HFSP_WRAP_MAGIC         0x4244
#define HFSP_WRAP_ATTRIB_SLOCK  0x8000
#define HFSP_WRAP_ATTRIB_SPARED 0x0200

#define HFSP_WRAPOFF_SIG          0x00
#define HFSP_WRAPOFF_ATTRIB       0x0A
#define HFSP_WRAPOFF_ABLKSIZE     0x14
#define HFSP_WRAPOFF_ABLKSTART    0x1C
#define HFSP_WRAPOFF_EMBEDSIG     0x7C
#define HFSP_WRAPOFF_EMBEDEXT     0x7E

#define HFSP_HIDDENDIR_NAME	"\xe2\x90\x80\xe2\x90\x80\xe2\x90\x80\xe2\x90\x80HFS+ Private Data"

#define HFSP_HARDLINK_TYPE	0x686c6e6b	/* 'hlnk' */
#define HFSP_HFSPLUS_CREATOR	0x6866732b	/* 'hfs+' */

#define HFSP_MOUNT_VERSION	0x482b4c78	/* 'H+Lx' */

/* Structures used on disk */

typedef u32 hfsplus_cnid;
typedef u16 hfsplus_unichr;

/* A "string" as used in filenames, etc. */
typedef struct {
	u16 length;
	hfsplus_unichr unicode[255];
} __packed hfsplus_unistr;

#define HFSPLUS_MAX_STRLEN 255

/* POSIX permissions */
typedef struct {
	u32 owner;
	u32 group;
	u32 mode;
	u32 dev;
} __packed hfsplus_perm;

/* A single contiguous area of a file */
typedef struct {
	u32 start_block;
	u32 block_count;
} __packed hfsplus_extent;
typedef hfsplus_extent hfsplus_extent_rec[8];

/* Information for a "Fork" in a file */
typedef struct {
	u64 total_size;
	u32 clump_size;
	u32 total_blocks;
	hfsplus_extent_rec extents;
} __packed hfsplus_fork_raw;

/* HFS+ Volume Header */
typedef struct hfsplus_vh {
	u16 signature;
	u16 version;
	u32 attributes;
	u32 last_mount_vers;
	u32 reserved;

	u32 create_date;
	u32 modify_date;
	u32 backup_date;
	u32 checked_date;

	u32 file_count;
	u32 folder_count;

	u32 blocksize;
	u32 total_blocks;
	u32 free_blocks;

	u32 next_alloc;
	u32 rsrc_clump_sz;
	u32 data_clump_sz;
	hfsplus_cnid next_cnid;

	u32 write_count;
	u64 encodings_bmp;

	u8 finder_info[32];

	hfsplus_fork_raw alloc_file;
	hfsplus_fork_raw ext_file;
	hfsplus_fork_raw cat_file;
	hfsplus_fork_raw attr_file;
	hfsplus_fork_raw start_file;
} __packed hfsplus_vh;

/* HFS+ volume attributes */
#define HFSPLUS_VOL_UNMNT     (1 << 8)
#define HFSPLUS_VOL_SPARE_BLK (1 << 9)
#define HFSPLUS_VOL_NOCACHE   (1 << 10)
#define HFSPLUS_VOL_INCNSTNT  (1 << 11)
#define HFSPLUS_VOL_SOFTLOCK  (1 << 15)

/* HFS+ BTree node descriptor */
typedef struct {
	u32 next;
	u32 prev;
	s8 kind;
	u8 height;
	u16 num_rec;
	u16 reserved;
} __packed hfsplus_btree_node_desc;

/* HFS+ BTree node types */
#define HFSPLUS_NODE_NDX  0x00
#define HFSPLUS_NODE_HEAD 0x01
#define HFSPLUS_NODE_MAP  0x02
#define HFSPLUS_NODE_LEAF 0xFF

/* HFS+ BTree header */
typedef struct {
	u16 depth;
	u32 root;
	u32 leaf_count;
	u32 leaf_head;
	u32 leaf_tail;
	u16 node_size;
	u16 max_key_len;
	u32 node_count;
	u32 free_nodes;
	u16 reserved1;
	u32 clump_size;
	u8 btree_type;
	u8 reserved2;
	u32 attributes;
	u32 reserved3[16];
} __packed hfsplus_btree_head;

/* BTree attributes */
#define HFSPLUS_TREE_BIGKEYS         2
#define HFSPLUS_TREE_VAR_NDXKEY_SIZE 4

/* HFS+ BTree misc info */
#define HFSPLUS_TREE_HEAD 0
#define HFSPLUS_NODE_MXSZ 32768

/* Some special File ID numbers (stolen from hfs.h) */
#define HFSPLUS_POR_CNID		1	/* Parent Of the Root */
#define HFSPLUS_ROOT_CNID		2	/* ROOT directory */
#define HFSPLUS_EXT_CNID		3	/* EXTents B-tree */
#define HFSPLUS_CAT_CNID		4	/* CATalog B-tree */
#define HFSPLUS_BAD_CNID		5	/* BAD blocks file */
#define HFSPLUS_ALLOC_CNID		6	/* ALLOCation file */
#define HFSPLUS_START_CNID		7	/* STARTup file */
#define HFSPLUS_ATTR_CNID		8	/* ATTRibutes file */
#define HFSPLUS_EXCH_CNID		15	/* ExchangeFiles temp id */
#define HFSPLUS_FIRSTUSER_CNID		16	/* first available user id */

/* HFS+ catalog entry key */
typedef struct {
	u16 key_len;
	hfsplus_cnid parent;
	hfsplus_unistr name;
} __packed hfsplus_cat_key;


/* Structs from hfs.h */
typedef struct {
	u16 v;
	u16 h;
} __packed hfsp_point;

typedef struct {
	u16 top;
	u16 left;
	u16 bottom;
	u16 right;
} __packed hfsp_rect;


/* HFS directory info (stolen from hfs.h */
typedef struct {
	hfsp_rect frRect;
	u16 frFlags;
	hfsp_point frLocation;
	u16 frView;
} __packed DInfo;

typedef struct {
	hfsp_point frScroll;
	u32 frOpenChain;
	u16 frUnused;
	u16 frComment;
	u32 frPutAway;
} __packed DXInfo;

/* HFS+ folder data (part of an hfsplus_cat_entry) */
typedef struct {
	s16 type;
	u16 flags;
	u32 valence;
	hfsplus_cnid id;
	u32 create_date;
	u32 content_mod_date;
	u32 attribute_mod_date;
	u32 access_date;
	u32 backup_date;
	hfsplus_perm permissions;
	DInfo user_info;
	DXInfo finder_info;
	u32 text_encoding;
	u32 reserved;
} __packed hfsplus_cat_folder;

/* HFS file info (stolen from hfs.h) */
typedef struct {
	u32 fdType;
	u32 fdCreator;
	u16 fdFlags;
	hfsp_point fdLocation;
	u16 fdFldr;
} __packed FInfo;

typedef struct {
	u16 fdIconID;
	u8 fdUnused[8];
	u16 fdComment;
	u32 fdPutAway;
} __packed FXInfo;

/* HFS+ file data (part of a cat_entry) */
typedef struct {
	s16 type;
	u16 flags;
	u32 reserved1;
	hfsplus_cnid id;
	u32 create_date;
	u32 content_mod_date;
	u32 attribute_mod_date;
	u32 access_date;
	u32 backup_date;
	hfsplus_perm permissions;
	FInfo user_info;
	FXInfo finder_info;
	u32 text_encoding;
	u32 reserved2;

	hfsplus_fork_raw data_fork;
	hfsplus_fork_raw rsrc_fork;
} __packed hfsplus_cat_file;

/* File attribute bits */
#define kHFSFileLockedBit       0x0000
#define kHFSFileLockedMask      0x0001
#define kHFSThreadExistsBit     0x0001
#define kHFSThreadExistsMask    0x0002

/* HFS+ catalog thread (part of a cat_entry) */
typedef struct {
	s16 type;
	s16 reserved;
	hfsplus_cnid parentID;
	hfsplus_unistr nodeName;
} __packed hfsplus_cat_thread;

#define HFSPLUS_MIN_THREAD_SZ 10

/* A data record in the catalog tree */
typedef union {
	s16 type;
	hfsplus_cat_folder folder;
	hfsplus_cat_file file;
	hfsplus_cat_thread thread;
} __packed hfsplus_cat_entry;

/* HFS+ catalog entry type */
#define HFSPLUS_FOLDER         0x0001
#define HFSPLUS_FILE           0x0002
#define HFSPLUS_FOLDER_THREAD  0x0003
#define HFSPLUS_FILE_THREAD    0x0004

/* HFS+ extents tree key */
typedef struct {
	u16 key_len;
	u8 fork_type;
	u8 pad;
	hfsplus_cnid cnid;
	u32 start_block;
} __packed hfsplus_ext_key;

#define HFSPLUS_EXT_KEYLEN 12

/* HFS+ generic BTree key */
typedef union {
	u16 key_len;
	hfsplus_cat_key cat;
	hfsplus_ext_key ext;
} __packed hfsplus_btree_key;

#endif
