/* -*- c -*- --------------------------------------------------------------- *
 *
 * linux/fs/autofs/init.c
 *
 *  Copyright 1997-1998 Transmeta Corporation -- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * ------------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/init.h>
#include "autofs_i.h"

static DECLARE_FSTYPE(autofs_fs_type, "autofs", autofs4_read_super, 0);

static int __init init_autofs4_fs(void)
{
	return register_filesystem(&autofs_fs_type);
}

static void __exit exit_autofs4_fs(void)
{
	unregister_filesystem(&autofs_fs_type);
}

module_init(init_autofs4_fs) 
module_exit(exit_autofs4_fs)
MODULE_LICENSE("GPL");
