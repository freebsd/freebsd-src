#ifndef _ASM_IA64_SN_HWGFS_H
#define _ASM_IA64_SN_HWGFS_H

/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
typedef struct dentry *hwgfs_handle_t;

extern hwgfs_handle_t hwgfs_register(hwgfs_handle_t dir, const char *name,
				     unsigned int flags,
				     unsigned int major, unsigned int minor,
				     umode_t mode, void *ops, void *info);
extern int hwgfs_mk_symlink(hwgfs_handle_t dir, const char *name,
			     unsigned int flags, const char *link,
			     hwgfs_handle_t *handle, void *info);
extern hwgfs_handle_t hwgfs_mk_dir(hwgfs_handle_t dir, const char *name,
				    void *info);
extern void hwgfs_unregister(hwgfs_handle_t de);

extern hwgfs_handle_t hwgfs_find_handle(hwgfs_handle_t dir, const char *name,
					unsigned int major,unsigned int minor,
					char type, int traverse_symlinks);
extern hwgfs_handle_t hwgfs_get_parent(hwgfs_handle_t de);
extern int hwgfs_generate_path(hwgfs_handle_t de, char *path, int buflen);

extern void *hwgfs_get_info(hwgfs_handle_t de);
extern int hwgfs_set_info(hwgfs_handle_t de, void *info);

#endif
