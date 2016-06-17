/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Dummy file for machines without standard floppy drives.
 *
 * Copyright (C) 1998 by Ralf Baechle
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/floppy.h>

/*
 * How to access the FDC's registers.
 */
static void no_fd_dummy(void)
{
	panic("no_fd_dummy called - shouldn't happen");
}

static unsigned long no_fd_getfdaddr1(void)
{
	return (unsigned long)-1;	/* No FDC nowhere ... */
}

static unsigned long no_fd_drive_type(unsigned long n)
{
	return 0;
}

struct fd_ops no_fd_ops = {
	/*
	 * How to access the floppy controller's ports
	 */
	(void *) no_fd_dummy,
	(void *) no_fd_dummy,
	/*
	 * How to access the floppy DMA functions.
	 */
	(void *) no_fd_dummy,
	(void *) no_fd_dummy,
	(void *) no_fd_dummy,
	(void *) no_fd_dummy,
	(void *) no_fd_dummy,
	(void *) no_fd_dummy,
	(void *) no_fd_dummy,
	(void *) no_fd_dummy,
	(void *) no_fd_dummy,
	(void *) no_fd_dummy,
	(void *) no_fd_dummy,
	no_fd_getfdaddr1,
	(void *) no_fd_dummy,
	(void *) no_fd_dummy,
	no_fd_drive_type
};
