/*-
 * Copyright 2000 Hans Reiser
 * See README for licensing and copyright details
 * 
 * Ported to FreeBSD by Jean-Sébastien Pédron <jspedron@club-internet.fr>
 * 
 * $FreeBSD$
 */

#ifndef _GNU_REISERFS_REISERFS_FS_I_H
#define _GNU_REISERFS_REISERFS_FS_I_H

#include <sys/queue.h>

/* Bitmasks for i_flags field in reiserfs-specific part of inode */
typedef enum {
	/*
	 * This says what format of key do all items (but stat data) of
	 * an object have.  If this is set, that format is 3.6 otherwise
	 * - 3.5
	 */
	i_item_key_version_mask		= 0x0001,
	/* If this is unset, object has 3.5 stat data, otherwise, it has
	 * 3.6 stat data with 64bit size, 32bit nlink etc. */
	i_stat_data_version_mask	= 0x0002,
	/* File might need tail packing on close */
	i_pack_on_close_mask		= 0x0004,
	/* Don't pack tail of file */
	i_nopack_mask			= 0x0008,
	/* If those is set, "safe link" was created for this file during
	 * truncate or unlink. Safe link is used to avoid leakage of disk
	 * space on crash with some files open, but unlinked. */
	i_link_saved_unlink_mask	= 0x0010,
	i_link_saved_truncate_mask	= 0x0020,
	i_priv_object			= 0x0080,
	i_has_xattr_dir			= 0x0100,
} reiserfs_inode_flags;

struct reiserfs_node {
	struct vnode	*i_vnode;
	struct vnode	*i_devvp;
	struct cdev	*i_dev;
	ino_t		 i_number;

	ino_t		 i_ino;

	struct reiserfs_sb_info *i_reiserfs;

	uint32_t	 i_flag;              /* Flags, see below */
	uint32_t	 i_key[4];            /* Key is still 4 32 bit
						 integers */
	uint32_t	 i_flags;             /* Transient inode flags that
						 are never stored on disk.
						 Bitmasks for this field
						 are defined above. */
	uint32_t	 i_first_direct_byte; /* Offset of first byte stored
						 in direct item. */
	uint32_t	 i_attrs;             /* Copy of persistent inode
						 flags read from sd_attrs. */

	uint16_t	 i_mode;              /* IFMT, permissions. */
	uint16_t	 i_nlink;             /* File link count. */
	uint64_t	 i_size;              /* File byte count. */
	uint32_t	 i_bytes;
	uid_t		 i_uid;               /* File owner. */
	gid_t		 i_gid;               /* File group. */
	struct timespec	 i_atime;             /* Last access time. */
	struct timespec	 i_mtime;             /* Last modified time. */
	struct timespec	 i_ctime;             /* Last inode change time. */

	uint32_t	 i_blocks;
	uint32_t	 i_generation;
};

#define	VTOI(vp)	((struct reiserfs_node *)(vp)->v_data)
#define	ITOV(ip)	((ip)->i_vnode)

/* These flags are kept in i_flag. */
#define	IN_HASHED	0x0020 /* Inode is on hash list */

/* This overlays the fid structure (see mount.h) */
struct rfid {
	uint16_t	rfid_len;   /* Length of structure */
	uint16_t	rfid_pad;   /* Force 32-bit alignment */
	ino_t		rfid_dirid; /* File key */
	ino_t		rfid_objectid;
	uint32_t	rfid_gen;   /* Generation number */
};

#endif /* !defined _GNU_REISERFS_REISERFS_FS_I_H */
