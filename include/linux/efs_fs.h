/*
 * efs_fs.h
 *
 * Copyright (c) 1999 Al Smith
 *
 * Portions derived from work (c) 1995,1996 Christian Vogelgsang.
 */

#ifndef __EFS_FS_H__
#define __EFS_FS_H__

#define EFS_VERSION "1.0a"

static const char cprt[] = "EFS: "EFS_VERSION" - (c) 1999 Al Smith <Al.Smith@aeschi.ch.eu.org>";

#include <asm/uaccess.h>

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#if LINUX_VERSION_CODE < 0x20200
#error This code is only for linux-2.2 and later.
#endif

/* 1 block is 512 bytes */
#define	EFS_BLOCKSIZE_BITS	9
#define	EFS_BLOCKSIZE		(1 << EFS_BLOCKSIZE_BITS)

#include <linux/efs_fs_i.h>
#include <linux/efs_dir.h>

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifdef _EFS_USE_GENERIC
#define INODE_INFO(i) (struct efs_inode_info *)	&((i)->u.generic_ip)
#define SUPER_INFO(s) (struct efs_sb_info *)	&((s)->u.generic_sbp)
#else
#define INODE_INFO(i)				&((i)->u.efs_i)
#define SUPER_INFO(s)				&((s)->u.efs_sb)
#endif

extern struct inode_operations efs_dir_inode_operations;
extern struct file_operations efs_dir_operations;
extern struct address_space_operations efs_symlink_aops;

extern struct super_block *efs_read_super(struct super_block *, void *, int);
extern int efs_statfs(struct super_block *, struct statfs *);

extern void efs_read_inode(struct inode *);
extern efs_block_t efs_map_block(struct inode *, efs_block_t);

extern struct dentry *efs_lookup(struct inode *, struct dentry *);
extern int efs_bmap(struct inode *, int);

#endif /* __EFS_FS_H__ */
