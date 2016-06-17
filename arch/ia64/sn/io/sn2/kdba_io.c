/*
 * Kernel Debugger Architecture Dependent POD functions.
 *
 * Copyright (C) 1999-2003 Silicon Graphics, Inc.  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#include <linux/types.h>
#include <linux/kdb.h>
//#include <linux/kdbprivate.h>

/**
 * kdba_io - enter POD mode from kdb
 * @argc: arg count
 * @argv: arg values
 * @envp: kdb env. vars
 * @regs: current register state
 *
 * Enter POD mode from kdb using SGI SN specific SAL function call.
 */
static int
kdba_io(int argc, const char **argv, const char **envp, struct pt_regs *regs)
{
	kdb_printf("kdba_io entered with addr 0x%p\n", (void *) regs);

        return(0);
}

/**
 * kdba_io_init - register 'io' command with kdb
 *
 * Register the 'io' command with kdb at load time.
 */
void
kdba_io_init(void)
{
        kdb_register("io", kdba_io, "<vaddr>", "Display IO Contents", 0);
}

/**
 * kdba_io_exit - unregister the 'io' command
 *
 * Tell kdb that the 'io' command is no longer available.
 */
static void __exit
kdba_exit(void)
{
        kdb_unregister("io");
}
