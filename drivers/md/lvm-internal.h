
/*
 * kernel/lvm_internal.h
 *
 * Copyright (C) 2001 Sistina Software
 *
 *
 * LVM driver is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * LVM driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

/*
 * Changelog
 *
 *    05/01/2001 - Factored this file out of lvm.c (Joe Thornber)
 *    11/01/2001 - Renamed lvm_internal and added declarations
 *                 for lvm_fs.c stuff
 *
 */

#ifndef LVM_INTERNAL_H
#define LVM_INTERNAL_H

#include <linux/lvm.h>

#define	_LVM_INTERNAL_H_VERSION	"LVM "LVM_RELEASE_NAME" ("LVM_RELEASE_DATE")"

/* global variables, defined in lvm.c */
extern char *lvm_version;
extern ushort lvm_iop_version;
extern int loadtime;
extern const char *const lvm_name;


extern uint vg_count;
extern vg_t *vg[];
extern struct file_operations lvm_chr_fops;

#ifndef	uchar
typedef unsigned char uchar;
#endif

extern struct block_device_operations lvm_blk_dops;

#define lvm_sectsize(dev) get_hardsect_size(dev)

/* 2.4.8 had no global min/max macros, and 2.4.9's were flawed */

/* debug macros */
#ifdef DEBUG_IOCTL
#define P_IOCTL(fmt, args...) printk(KERN_DEBUG "lvm ioctl: " fmt, ## args)
#else
#define P_IOCTL(fmt, args...)
#endif

#ifdef DEBUG_MAP
#define P_MAP(fmt, args...) printk(KERN_DEBUG "lvm map: " fmt, ## args)
#else
#define P_MAP(fmt, args...)
#endif

#ifdef DEBUG_KFREE
#define P_KFREE(fmt, args...) printk(KERN_DEBUG "lvm kfree: " fmt, ## args)
#else
#define P_KFREE(fmt, args...)
#endif

#ifdef DEBUG_DEVICE
#define P_DEV(fmt, args...) printk(KERN_DEBUG "lvm device: " fmt, ## args)
#else
#define P_DEV(fmt, args...)
#endif


/* lvm-snap.c */
int lvm_get_blksize(kdev_t);
int lvm_snapshot_alloc(lv_t *);
int lvm_snapshot_fill_COW_page(vg_t *, lv_t *);
int lvm_snapshot_COW(kdev_t, ulong, ulong, ulong, vg_t * vg, lv_t *);
int lvm_snapshot_remap_block(kdev_t *, ulong *, ulong, lv_t *);
void lvm_snapshot_release(lv_t *);
int lvm_write_COW_table_block(vg_t *, lv_t *);
void lvm_hash_link(lv_block_exception_t *, kdev_t, ulong, lv_t *);
int lvm_snapshot_alloc_hash_table(lv_t *);
void lvm_drop_snapshot(vg_t * vg, lv_t *, const char *);


/* lvm_fs.c */
void lvm_init_fs(void);
void lvm_fin_fs(void);

void lvm_fs_create_vg(vg_t * vg_ptr);
void lvm_fs_remove_vg(vg_t * vg_ptr);
devfs_handle_t lvm_fs_create_lv(vg_t * vg_ptr, lv_t * lv);
void lvm_fs_remove_lv(vg_t * vg_ptr, lv_t * lv);
void lvm_fs_create_pv(vg_t * vg_ptr, pv_t * pv);
void lvm_fs_remove_pv(vg_t * vg_ptr, pv_t * pv);

#endif
