/*
 * Copyright 2000-2002 by Hans Reiser, licensing governed by reiserfs/README
 */
#ifndef _REISER_FS_I
#define _REISER_FS_I

#include <linux/list.h>

/** bitmasks for i_flags field in reiserfs-specific part of inode */
typedef enum {
    /** this says what format of key do all items (but stat data) of
	an object have.  If this is set, that format is 3.6 otherwise
	- 3.5 */
    i_item_key_version_mask    =  0x0001,
    /** If this is unset, object has 3.5 stat data, otherwise, it has
	3.6 stat data with 64bit size, 32bit nlink etc. */
    i_stat_data_version_mask   =  0x0002,
    /** file might need tail packing on close */
    i_pack_on_close_mask       =  0x0004,
    /** don't pack tail of file */
    i_nopack_mask              =  0x0008,
    /** If those is set, "safe link" was created for this file during
	truncate or unlink. Safe link is used to avoid leakage of disk
	space on crash with some files open, but unlinked. */
    i_link_saved_unlink_mask   =  0x0010,
    i_link_saved_truncate_mask =  0x0020
} reiserfs_inode_flags;


struct reiserfs_inode_info {
    __u32 i_key [4];/* key is still 4 32 bit integers */
  
    /** transient inode flags that are never stored on disk. Bitmasks
	for this field are defined above. */
    __u32 i_flags;

    __u32 i_first_direct_byte; // offset of first byte stored in direct item.

    /* copy of persistent inode flags read from sd_attrs. */
    __u32 i_attrs;

    int i_prealloc_block; /* first unused block of a sequence of unused blocks */
    int i_prealloc_count; /* length of that sequence */
    struct list_head i_prealloc_list;	/* per-transaction list of inodes which
					 * have preallocated blocks */
  
    int new_packing_locality:1;		/* new_packig_locality is created; new blocks
					 * for the contents of this directory should be
					 * displaced */

    /* we use these for fsync or O_SYNC to decide which transaction
    ** needs to be committed in order for this inode to be properly
    ** flushed */
    unsigned long i_trans_id ;
    unsigned long i_trans_index ;

    /* direct io needs to make sure the tail is on disk to avoid
     * buffer alias problems.  This records the transaction last
     * involved in a direct->indirect conversion for this file
     */
    unsigned long i_tail_trans_id;
    unsigned long i_tail_trans_index;
};

#endif

