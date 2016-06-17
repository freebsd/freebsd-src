/*
 * JFFS -- Journaling Flash File System, Linux implementation.
 *
 * Copyright (C) 1999, 2000  Axis Communications AB.
 *
 * Created by Finn Hakansson <finn@axis.com>.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Id: intrep.h,v 1.14 2001/09/23 23:28:37 dwmw2 Exp $
 *
 */

#ifndef __LINUX_JFFS_INTREP_H__
#define __LINUX_JFFS_INTREP_H__
#include "jffs_fm.h"
struct jffs_node *jffs_alloc_node(void);
void jffs_free_node(struct jffs_node *n);
int jffs_get_node_inuse(void);
long jffs_get_file_count(void);

__u32 jffs_checksum(const void *data, int size);

void jffs_cleanup_control(struct jffs_control *c);
int jffs_build_fs(struct super_block *sb);

int jffs_insert_node(struct jffs_control *c, struct jffs_file *f,
		     const struct jffs_raw_inode *raw_inode,
		     const char *name, struct jffs_node *node);
struct jffs_file *jffs_find_file(struct jffs_control *c, __u32 ino);
struct jffs_file *jffs_find_child(struct jffs_file *dir, const char *name, int len);

void jffs_free_node(struct jffs_node *node);

int jffs_foreach_file(struct jffs_control *c, int (*func)(struct jffs_file *));
int jffs_free_node_list(struct jffs_file *f);
int jffs_free_file(struct jffs_file *f);
int jffs_possibly_delete_file(struct jffs_file *f);
int jffs_build_file(struct jffs_file *f);
int jffs_insert_file_into_hash(struct jffs_file *f);
int jffs_insert_file_into_tree(struct jffs_file *f);
int jffs_unlink_file_from_hash(struct jffs_file *f);
int jffs_unlink_file_from_tree(struct jffs_file *f);
int jffs_remove_redundant_nodes(struct jffs_file *f);
int jffs_file_count(struct jffs_file *f);

int jffs_write_node(struct jffs_control *c, struct jffs_node *node,
		    struct jffs_raw_inode *raw_inode,
		    const char *name, const unsigned char *buf,
		    int recoverable, struct jffs_file *f);
int jffs_read_data(struct jffs_file *f, unsigned char *buf, __u32 read_offset, __u32 size);

/* Garbage collection stuff.  */
int jffs_garbage_collect_thread(void *c);
void jffs_garbage_collect_trigger(struct jffs_control *c);
int jffs_garbage_collect_now(struct jffs_control *c);

/* Is there enough space on the flash?  */
static inline int JFFS_ENOUGH_SPACE(struct jffs_control *c, __u32 space)
{
	struct jffs_fmcontrol *fmc = c->fmc;

	while (1) {
		if ((fmc->flash_size - (fmc->used_size + fmc->dirty_size)) 
			>= fmc->min_free_size + space) {
			return 1;
		}
		if (fmc->dirty_size < fmc->sector_size)
			return 0;

		if (jffs_garbage_collect_now(c)) {
		  D1(printk("JFFS_ENOUGH_SPACE: jffs_garbage_collect_now() failed.\n"));
		  return 0;
		}
	}
}

/* For debugging purposes.  */
void jffs_print_node(struct jffs_node *n);
void jffs_print_raw_inode(struct jffs_raw_inode *raw_inode);
int jffs_print_file(struct jffs_file *f);
void jffs_print_hash_table(struct jffs_control *c);
void jffs_print_tree(struct jffs_file *first_file, int indent);

struct buffer_head *jffs_get_write_buffer(kdev_t dev, int block);
void jffs_put_write_buffer(struct buffer_head *bh);

#endif /* __LINUX_JFFS_INTREP_H__  */
