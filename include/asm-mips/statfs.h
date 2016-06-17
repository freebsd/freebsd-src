/*
 * Definitions for the statfs(2) call.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995 by Ralf Baechle
 */
#ifndef __ASM_MIPS_STATFS_H
#define __ASM_MIPS_STATFS_H

#include <linux/posix_types.h>

#ifndef __KERNEL_STRICT_NAMES

#include <linux/types.h>

typedef __kernel_fsid_t        fsid_t;

#endif

struct statfs {
	long		f_type;
#define f_fstyp f_type
	long		f_bsize;
	long		f_frsize;	/* Fragment size - unsupported */
	long		f_blocks;
	long		f_bfree;
	long		f_files;
	long		f_ffree;

	/* Linux specials */
	long	f_bavail;
	__kernel_fsid_t	f_fsid;
	long		f_namelen;
	long		f_spare[6];
};

#endif /* __ASM_MIPS_STATFS_H */
