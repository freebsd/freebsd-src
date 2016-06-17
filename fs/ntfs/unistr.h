/*
 * unistr.h - Exports for unicode string handling. Part of the Linux-NTFS
 *	      project.
 *
 * Copyright (c) 2000,2001 Anton Altaparmakov.
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be 
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty 
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS 
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LINUX_NTFS_UNISTR_H
#define _LINUX_NTFS_UNISTR_H

#include <linux/types.h>
#include <linux/nls.h>

extern const __u8 legal_ansi_char_array[0x40];

int ntfs_are_names_equal(wchar_t *s1, size_t s1_len,
			  wchar_t *s2, size_t s2_len, int ic,
			  wchar_t *upcase, __u32 upcase_size);

int ntfs_collate_names(wchar_t *upcase, __u32 upcase_len,
		       wchar_t *name1, __u32 name1_len,
		       wchar_t *name2, __u32 name2_len,
		       int ic, int err_val);

int ntfs_wcsncasecmp(wchar_t *s1, wchar_t *s2, size_t n,
		     wchar_t *upcase, __u32 upcase_size);

#endif /* defined _LINUX_NTFS_UNISTR_H */

