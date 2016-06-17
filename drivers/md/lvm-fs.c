/*
 * kernel/lvm-fs.c
 *
 * Copyright (C) 2001-2002 Sistina Software
 *
 * January-May,December 2001
 * May 2002
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
 *    11/01/2001 - First version (Joe Thornber)
 *    21/03/2001 - added display of stripes and stripe size (HM)
 *    04/10/2001 - corrected devfs_register() call in lvm_init_fs()
 *    11/04/2001 - don't devfs_register("lvm") as user-space always does it
 *    10/05/2001 - show more of PV name in /proc/lvm/global
 *    16/12/2001 - fix devfs unregister order and prevent duplicate unreg (REG)
 *
 */

#include <linux/config.h>
#include <linux/version.h>

#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/smp_lock.h>

#include <linux/devfs_fs_kernel.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/lvm.h>

#include "lvm-internal.h"


static int _proc_read_vg(char *page, char **start, off_t off,
			 int count, int *eof, void *data);
static int _proc_read_lv(char *page, char **start, off_t off,
			 int count, int *eof, void *data);
static int _proc_read_pv(char *page, char **start, off_t off,
			 int count, int *eof, void *data);
static int _proc_read_global(char *page, char **start, off_t off,
			     int count, int *eof, void *data);

static int _vg_info(vg_t * vg_ptr, char *buf);
static int _lv_info(vg_t * vg_ptr, lv_t * lv_ptr, char *buf);
static int _pv_info(pv_t * pv_ptr, char *buf);

static void _show_uuid(const char *src, char *b, char *e);

#if 0
static devfs_handle_t lvm_devfs_handle;
#endif
static devfs_handle_t vg_devfs_handle[MAX_VG];
static devfs_handle_t ch_devfs_handle[MAX_VG];
static devfs_handle_t lv_devfs_handle[MAX_LV];

static struct proc_dir_entry *lvm_proc_dir = NULL;
static struct proc_dir_entry *lvm_proc_vg_subdir = NULL;

/* inline functions */

/* public interface */
void __init lvm_init_fs()
{
	struct proc_dir_entry *pde;

/* User-space has already registered this */
#if 0
	lvm_devfs_handle = devfs_register(0, "lvm", 0, LVM_CHAR_MAJOR, 0,
					  S_IFCHR | S_IRUSR | S_IWUSR |
					  S_IRGRP, &lvm_chr_fops, NULL);
#endif
	lvm_proc_dir = create_proc_entry(LVM_DIR, S_IFDIR, &proc_root);
	if (lvm_proc_dir) {
		lvm_proc_vg_subdir =
		    create_proc_entry(LVM_VG_SUBDIR, S_IFDIR,
				      lvm_proc_dir);
		pde = create_proc_entry(LVM_GLOBAL, S_IFREG, lvm_proc_dir);
		if (pde != NULL)
			pde->read_proc = _proc_read_global;
	}
}

void lvm_fin_fs()
{
#if 0
	devfs_unregister(lvm_devfs_handle);
#endif
	remove_proc_entry(LVM_GLOBAL, lvm_proc_dir);
	remove_proc_entry(LVM_VG_SUBDIR, lvm_proc_dir);
	remove_proc_entry(LVM_DIR, &proc_root);
}

void lvm_fs_create_vg(vg_t * vg_ptr)
{
	struct proc_dir_entry *pde;

	if (!vg_ptr)
		return;

	vg_devfs_handle[vg_ptr->vg_number] =
	    devfs_mk_dir(0, vg_ptr->vg_name, NULL);

	ch_devfs_handle[vg_ptr->vg_number] =
	    devfs_register(vg_devfs_handle[vg_ptr->vg_number], "group",
			   DEVFS_FL_DEFAULT, LVM_CHAR_MAJOR,
			   vg_ptr->vg_number,
			   S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP,
			   &lvm_chr_fops, NULL);

	vg_ptr->vg_dir_pde = create_proc_entry(vg_ptr->vg_name, S_IFDIR,
					       lvm_proc_vg_subdir);

	if ((pde =
	     create_proc_entry("group", S_IFREG, vg_ptr->vg_dir_pde))) {
		pde->read_proc = _proc_read_vg;
		pde->data = vg_ptr;
	}

	vg_ptr->lv_subdir_pde =
	    create_proc_entry(LVM_LV_SUBDIR, S_IFDIR, vg_ptr->vg_dir_pde);

	vg_ptr->pv_subdir_pde =
	    create_proc_entry(LVM_PV_SUBDIR, S_IFDIR, vg_ptr->vg_dir_pde);
}

void lvm_fs_remove_vg(vg_t * vg_ptr)
{
	int i;

	if (!vg_ptr)
		return;

	devfs_unregister(ch_devfs_handle[vg_ptr->vg_number]);
	ch_devfs_handle[vg_ptr->vg_number] = NULL;

	/* remove lv's */
	for (i = 0; i < vg_ptr->lv_max; i++)
		if (vg_ptr->lv[i])
			lvm_fs_remove_lv(vg_ptr, vg_ptr->lv[i]);

	/* must not remove directory before leaf nodes */
	devfs_unregister(vg_devfs_handle[vg_ptr->vg_number]);
	vg_devfs_handle[vg_ptr->vg_number] = NULL;

	/* remove pv's */
	for (i = 0; i < vg_ptr->pv_max; i++)
		if (vg_ptr->pv[i])
			lvm_fs_remove_pv(vg_ptr, vg_ptr->pv[i]);

	if (vg_ptr->vg_dir_pde) {
		remove_proc_entry(LVM_LV_SUBDIR, vg_ptr->vg_dir_pde);
		vg_ptr->lv_subdir_pde = NULL;

		remove_proc_entry(LVM_PV_SUBDIR, vg_ptr->vg_dir_pde);
		vg_ptr->pv_subdir_pde = NULL;

		remove_proc_entry("group", vg_ptr->vg_dir_pde);
		vg_ptr->vg_dir_pde = NULL;

		remove_proc_entry(vg_ptr->vg_name, lvm_proc_vg_subdir);
	}
}


static inline const char *_basename(const char *str)
{
	const char *name = strrchr(str, '/');
	name = name ? name + 1 : str;
	return name;
}

devfs_handle_t lvm_fs_create_lv(vg_t * vg_ptr, lv_t * lv)
{
	struct proc_dir_entry *pde;
	const char *name;

	if (!vg_ptr || !lv)
		return NULL;

	name = _basename(lv->lv_name);

	lv_devfs_handle[MINOR(lv->lv_dev)] =
	    devfs_register(vg_devfs_handle[vg_ptr->vg_number], name,
			   DEVFS_FL_DEFAULT, LVM_BLK_MAJOR,
			   MINOR(lv->lv_dev),
			   S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP,
			   &lvm_blk_dops, NULL);

	if (vg_ptr->lv_subdir_pde &&
	    (pde =
	     create_proc_entry(name, S_IFREG, vg_ptr->lv_subdir_pde))) {
		pde->read_proc = _proc_read_lv;
		pde->data = lv;
	}
	return lv_devfs_handle[MINOR(lv->lv_dev)];
}

void lvm_fs_remove_lv(vg_t * vg_ptr, lv_t * lv)
{

	if (!vg_ptr || !lv)
		return;

	devfs_unregister(lv_devfs_handle[MINOR(lv->lv_dev)]);
	lv_devfs_handle[MINOR(lv->lv_dev)] = NULL;

	if (vg_ptr->lv_subdir_pde) {
		const char *name = _basename(lv->lv_name);
		remove_proc_entry(name, vg_ptr->lv_subdir_pde);
	}
}


static inline void _make_pv_name(const char *src, char *b, char *e)
{
	int offset = strlen(LVM_DIR_PREFIX);
	if (strncmp(src, LVM_DIR_PREFIX, offset))
		offset = 0;

	e--;
	src += offset;
	while (*src && (b != e)) {
		*b++ = (*src == '/') ? '_' : *src;
		src++;
	}
	*b = '\0';
}

void lvm_fs_create_pv(vg_t * vg_ptr, pv_t * pv)
{
	struct proc_dir_entry *pde;
	char name[NAME_LEN];

	if (!vg_ptr || !pv)
		return;

	if (!vg_ptr->pv_subdir_pde)
		return;

	_make_pv_name(pv->pv_name, name, name + sizeof(name));
	if ((pde =
	     create_proc_entry(name, S_IFREG, vg_ptr->pv_subdir_pde))) {
		pde->read_proc = _proc_read_pv;
		pde->data = pv;
	}
}

void lvm_fs_remove_pv(vg_t * vg_ptr, pv_t * pv)
{
	char name[NAME_LEN];

	if (!vg_ptr || !pv)
		return;

	if (!vg_ptr->pv_subdir_pde)
		return;

	_make_pv_name(pv->pv_name, name, name + sizeof(name));
	remove_proc_entry(name, vg_ptr->pv_subdir_pde);
}


static int _proc_read_vg(char *page, char **start, off_t off,
			 int count, int *eof, void *data)
{
	int sz = 0;
	vg_t *vg_ptr = data;
	char uuid[NAME_LEN];

	sz += sprintf(page + sz, "name:         %s\n", vg_ptr->vg_name);
	sz += sprintf(page + sz, "size:         %u\n",
		      vg_ptr->pe_total * vg_ptr->pe_size / 2);
	sz += sprintf(page + sz, "access:       %u\n", vg_ptr->vg_access);
	sz += sprintf(page + sz, "status:       %u\n", vg_ptr->vg_status);
	sz += sprintf(page + sz, "number:       %u\n", vg_ptr->vg_number);
	sz += sprintf(page + sz, "LV max:       %u\n", vg_ptr->lv_max);
	sz += sprintf(page + sz, "LV current:   %u\n", vg_ptr->lv_cur);
	sz += sprintf(page + sz, "LV open:      %u\n", vg_ptr->lv_open);
	sz += sprintf(page + sz, "PV max:       %u\n", vg_ptr->pv_max);
	sz += sprintf(page + sz, "PV current:   %u\n", vg_ptr->pv_cur);
	sz += sprintf(page + sz, "PV active:    %u\n", vg_ptr->pv_act);
	sz +=
	    sprintf(page + sz, "PE size:      %u\n", vg_ptr->pe_size / 2);
	sz += sprintf(page + sz, "PE total:     %u\n", vg_ptr->pe_total);
	sz +=
	    sprintf(page + sz, "PE allocated: %u\n", vg_ptr->pe_allocated);

	_show_uuid(vg_ptr->vg_uuid, uuid, uuid + sizeof(uuid));
	sz += sprintf(page + sz, "uuid:         %s\n", uuid);

	return sz;
}

static int _proc_read_lv(char *page, char **start, off_t off,
			 int count, int *eof, void *data)
{
	int sz = 0;
	lv_t *lv = data;

	sz += sprintf(page + sz, "name:         %s\n", lv->lv_name);
	sz += sprintf(page + sz, "size:         %u\n", lv->lv_size);
	sz += sprintf(page + sz, "access:       %u\n", lv->lv_access);
	sz += sprintf(page + sz, "status:       %u\n", lv->lv_status);
	sz += sprintf(page + sz, "number:       %u\n", lv->lv_number);
	sz += sprintf(page + sz, "open:         %u\n", lv->lv_open);
	sz += sprintf(page + sz, "allocation:   %u\n", lv->lv_allocation);
	if (lv->lv_stripes > 1) {
		sz += sprintf(page + sz, "stripes:      %u\n",
			      lv->lv_stripes);
		sz += sprintf(page + sz, "stripesize:   %u\n",
			      lv->lv_stripesize);
	}
	sz += sprintf(page + sz, "device:       %02u:%02u\n",
		      MAJOR(lv->lv_dev), MINOR(lv->lv_dev));

	return sz;
}

static int _proc_read_pv(char *page, char **start, off_t off,
			 int count, int *eof, void *data)
{
	int sz = 0;
	pv_t *pv = data;
	char uuid[NAME_LEN];

	sz += sprintf(page + sz, "name:         %s\n", pv->pv_name);
	sz += sprintf(page + sz, "size:         %u\n", pv->pv_size);
	sz += sprintf(page + sz, "status:       %u\n", pv->pv_status);
	sz += sprintf(page + sz, "number:       %u\n", pv->pv_number);
	sz += sprintf(page + sz, "allocatable:  %u\n", pv->pv_allocatable);
	sz += sprintf(page + sz, "LV current:   %u\n", pv->lv_cur);
	sz += sprintf(page + sz, "PE size:      %u\n", pv->pe_size / 2);
	sz += sprintf(page + sz, "PE total:     %u\n", pv->pe_total);
	sz += sprintf(page + sz, "PE allocated: %u\n", pv->pe_allocated);
	sz += sprintf(page + sz, "device:       %02u:%02u\n",
		      MAJOR(pv->pv_dev), MINOR(pv->pv_dev));

	_show_uuid(pv->pv_uuid, uuid, uuid + sizeof(uuid));
	sz += sprintf(page + sz, "uuid:         %s\n", uuid);

	return sz;
}

static int _proc_read_global(char *page, char **start, off_t pos,
			     int count, int *eof, void *data)
{

#define  LVM_PROC_BUF   ( i == 0 ? dummy_buf : &buf[sz])

	int c, i, l, p, v, vg_counter, pv_counter, lv_counter,
	    lv_open_counter, lv_open_total, pe_t_bytes, hash_table_bytes,
	    lv_block_exception_t_bytes, seconds;
	static off_t sz;
	off_t sz_last;
	static char *buf = NULL;
	static char dummy_buf[160];	/* sized for 2 lines */
	vg_t *vg_ptr;
	lv_t *lv_ptr;
	pv_t *pv_ptr;


#ifdef DEBUG_LVM_PROC_GET_INFO
	printk(KERN_DEBUG
	       "%s - lvm_proc_get_global_info CALLED  pos: %lu  count: %d\n",
	       lvm_name, pos, count);
#endif

	if (pos != 0 && buf != NULL)
		goto out;

	sz_last = vg_counter = pv_counter = lv_counter = lv_open_counter =
	    lv_open_total = pe_t_bytes = hash_table_bytes =
	    lv_block_exception_t_bytes = 0;

	/* get some statistics */
	for (v = 0; v < ABS_MAX_VG; v++) {
		if ((vg_ptr = vg[v]) != NULL) {
			vg_counter++;
			pv_counter += vg_ptr->pv_cur;
			lv_counter += vg_ptr->lv_cur;
			if (vg_ptr->lv_cur > 0) {
				for (l = 0; l < vg[v]->lv_max; l++) {
					if ((lv_ptr =
					     vg_ptr->lv[l]) != NULL) {
						pe_t_bytes +=
						    lv_ptr->
						    lv_allocated_le;
						hash_table_bytes +=
						    lv_ptr->
						    lv_snapshot_hash_table_size;
						if (lv_ptr->
						    lv_block_exception !=
						    NULL)
							lv_block_exception_t_bytes
							    +=
							    lv_ptr->
							    lv_remap_end;
						if (lv_ptr->lv_open > 0) {
							lv_open_counter++;
							lv_open_total +=
							    lv_ptr->
							    lv_open;
						}
					}
				}
			}
		}
	}

	pe_t_bytes *= sizeof(pe_t);
	lv_block_exception_t_bytes *= sizeof(lv_block_exception_t);

	if (buf != NULL) {
		P_KFREE("%s -- vfree %d\n", lvm_name, __LINE__);
		lock_kernel();
		vfree(buf);
		unlock_kernel();
		buf = NULL;
	}
	/* 2 times: first to get size to allocate buffer,
	   2nd to fill the malloced buffer */
	for (i = 0; i < 2; i++) {
		sz = 0;
		sz += sprintf(LVM_PROC_BUF, "LVM "
#ifdef MODULE
			      "module"
#else
			      "driver"
#endif
			      " %s\n\n"
			      "Total:  %d VG%s  %d PV%s  %d LV%s ",
			      lvm_version,
			      vg_counter, vg_counter == 1 ? "" : "s",
			      pv_counter, pv_counter == 1 ? "" : "s",
			      lv_counter, lv_counter == 1 ? "" : "s");
		sz += sprintf(LVM_PROC_BUF,
			      "(%d LV%s open",
			      lv_open_counter,
			      lv_open_counter == 1 ? "" : "s");
		if (lv_open_total > 0)
			sz += sprintf(LVM_PROC_BUF,
				      " %d times)\n", lv_open_total);
		else
			sz += sprintf(LVM_PROC_BUF, ")");
		sz += sprintf(LVM_PROC_BUF,
			      "\nGlobal: %lu bytes malloced   IOP version: %d   ",
			      vg_counter * sizeof(vg_t) +
			      pv_counter * sizeof(pv_t) +
			      lv_counter * sizeof(lv_t) +
			      pe_t_bytes + hash_table_bytes +
			      lv_block_exception_t_bytes + sz_last,
			      lvm_iop_version);

		seconds = CURRENT_TIME - loadtime;
		if (seconds < 0)
			loadtime = CURRENT_TIME + seconds;
		if (seconds / 86400 > 0) {
			sz += sprintf(LVM_PROC_BUF, "%d day%s ",
				      seconds / 86400,
				      seconds / 86400 == 0 ||
				      seconds / 86400 > 1 ? "s" : "");
		}
		sz += sprintf(LVM_PROC_BUF, "%d:%02d:%02d active\n",
			      (seconds % 86400) / 3600,
			      (seconds % 3600) / 60, seconds % 60);

		if (vg_counter > 0) {
			for (v = 0; v < ABS_MAX_VG; v++) {
				/* volume group */
				if ((vg_ptr = vg[v]) != NULL) {
					sz +=
					    _vg_info(vg_ptr, LVM_PROC_BUF);

					/* physical volumes */
					sz += sprintf(LVM_PROC_BUF,
						      "\n  PV%s ",
						      vg_ptr->pv_cur ==
						      1 ? ": " : "s:");
					c = 0;
					for (p = 0; p < vg_ptr->pv_max;
					     p++) {
						if ((pv_ptr =
						     vg_ptr->pv[p]) !=
						    NULL) {
							sz +=
							    _pv_info
							    (pv_ptr,
							     LVM_PROC_BUF);

							c++;
							if (c <
							    vg_ptr->pv_cur)
								sz +=
								    sprintf
								    (LVM_PROC_BUF,
								     "\n       ");
						}
					}

					/* logical volumes */
					sz += sprintf(LVM_PROC_BUF,
						      "\n    LV%s ",
						      vg_ptr->lv_cur ==
						      1 ? ": " : "s:");
					c = 0;
					for (l = 0; l < vg_ptr->lv_max;
					     l++) {
						if ((lv_ptr =
						     vg_ptr->lv[l]) !=
						    NULL) {
							sz +=
							    _lv_info
							    (vg_ptr,
							     lv_ptr,
							     LVM_PROC_BUF);
							c++;
							if (c <
							    vg_ptr->lv_cur)
								sz +=
								    sprintf
								    (LVM_PROC_BUF,
								     "\n         ");
						}
					}
					if (vg_ptr->lv_cur == 0)
						sz +=
						    sprintf(LVM_PROC_BUF,
							    "none");
					sz += sprintf(LVM_PROC_BUF, "\n");
				}
			}
		}
		if (buf == NULL) {
			lock_kernel();
			buf = vmalloc(sz);
			unlock_kernel();
			if (buf == NULL) {
				sz = 0;
				return sprintf(page,
					       "%s - vmalloc error at line %d\n",
					       lvm_name, __LINE__);
			}
		}
		sz_last = sz;
	}

      out:
	if (pos > sz - 1) {
		lock_kernel();
		vfree(buf);
		unlock_kernel();
		buf = NULL;
		return 0;
	}
	*start = &buf[pos];
	if (sz - pos < count)
		return sz - pos;
	else
		return count;

#undef LVM_PROC_BUF
}

/*
 * provide VG info for proc filesystem use (global)
 */
static int _vg_info(vg_t * vg_ptr, char *buf)
{
	int sz = 0;
	char inactive_flag = ' ';

	if (!(vg_ptr->vg_status & VG_ACTIVE))
		inactive_flag = 'I';
	sz = sprintf(buf,
		     "\nVG: %c%s  [%d PV, %d LV/%d open] "
		     " PE Size: %d KB\n"
		     "  Usage [KB/PE]: %d /%d total  "
		     "%d /%d used  %d /%d free",
		     inactive_flag,
		     vg_ptr->vg_name,
		     vg_ptr->pv_cur,
		     vg_ptr->lv_cur,
		     vg_ptr->lv_open,
		     vg_ptr->pe_size >> 1,
		     vg_ptr->pe_size * vg_ptr->pe_total >> 1,
		     vg_ptr->pe_total,
		     vg_ptr->pe_allocated * vg_ptr->pe_size >> 1,
		     vg_ptr->pe_allocated,
		     (vg_ptr->pe_total - vg_ptr->pe_allocated) *
		     vg_ptr->pe_size >> 1,
		     vg_ptr->pe_total - vg_ptr->pe_allocated);
	return sz;
}


/*
 * provide LV info for proc filesystem use (global)
 */
static int _lv_info(vg_t * vg_ptr, lv_t * lv_ptr, char *buf)
{
	int sz = 0;
	char inactive_flag = 'A', allocation_flag = ' ',
	    stripes_flag = ' ', rw_flag = ' ', *basename;

	if (!(lv_ptr->lv_status & LV_ACTIVE))
		inactive_flag = 'I';
	rw_flag = 'R';
	if (lv_ptr->lv_access & LV_WRITE)
		rw_flag = 'W';
	allocation_flag = 'D';
	if (lv_ptr->lv_allocation & LV_CONTIGUOUS)
		allocation_flag = 'C';
	stripes_flag = 'L';
	if (lv_ptr->lv_stripes > 1)
		stripes_flag = 'S';
	sz += sprintf(buf + sz,
		      "[%c%c%c%c",
		      inactive_flag,
		      rw_flag, allocation_flag, stripes_flag);
	if (lv_ptr->lv_stripes > 1)
		sz += sprintf(buf + sz, "%-2d", lv_ptr->lv_stripes);
	else
		sz += sprintf(buf + sz, "  ");

	/* FIXME: use _basename */
	basename = strrchr(lv_ptr->lv_name, '/');
	if (basename == 0)
		basename = lv_ptr->lv_name;
	else
		basename++;
	sz += sprintf(buf + sz, "] %-25s", basename);
	if (strlen(basename) > 25)
		sz += sprintf(buf + sz,
			      "\n                              ");
	sz += sprintf(buf + sz, "%9d /%-6d   ",
		      lv_ptr->lv_size >> 1,
		      lv_ptr->lv_size / vg_ptr->pe_size);

	if (lv_ptr->lv_open == 0)
		sz += sprintf(buf + sz, "close");
	else
		sz += sprintf(buf + sz, "%dx open", lv_ptr->lv_open);

	return sz;
}


/*
 * provide PV info for proc filesystem use (global)
 */
static int _pv_info(pv_t * pv, char *buf)
{
	int sz = 0;
	char inactive_flag = 'A', allocation_flag = ' ';
	char *pv_name = NULL;

	if (!(pv->pv_status & PV_ACTIVE))
		inactive_flag = 'I';
	allocation_flag = 'A';
	if (!(pv->pv_allocatable & PV_ALLOCATABLE))
		allocation_flag = 'N';
	pv_name = strchr(pv->pv_name + 1, '/');
	if (pv_name == 0)
		pv_name = pv->pv_name;
	else
		pv_name++;
	sz = sprintf(buf,
		     "[%c%c] %-21s %8d /%-6d  "
		     "%8d /%-6d  %8d /%-6d",
		     inactive_flag,
		     allocation_flag,
		     pv_name,
		     pv->pe_total * pv->pe_size >> 1,
		     pv->pe_total,
		     pv->pe_allocated * pv->pe_size >> 1,
		     pv->pe_allocated,
		     (pv->pe_total - pv->pe_allocated) *
		     pv->pe_size >> 1, pv->pe_total - pv->pe_allocated);
	return sz;
}

static void _show_uuid(const char *src, char *b, char *e)
{
	int i;

	e--;
	for (i = 0; *src && (b != e); i++) {
		if (i && !(i & 0x3))
			*b++ = '-';
		*b++ = *src++;
	}
	*b = '\0';
}
