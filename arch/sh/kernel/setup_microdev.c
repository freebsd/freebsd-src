/* 
 * linux/arch/sh/kernel/setup_microdev.c
 *
 * Copyright (C) 2003 Sean McGoogan (Sean.McGoogan@superh.com)
 *
 * SuperH SH4-202 MicroDev board support.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <asm/io.h>

	/* General-Purpose base address on CPU-board FPGA */
#define	MICRODEV_FPGA_GP_BASE		0xa6100000ul

	/* Address of Cache Control Register */
#define CCR				0xff00001cul

/*
 * Initialize the board
 */
void __init setup_microdev(void)
{
	int * const fpgaRevisionRegister = (int*)(MICRODEV_FPGA_GP_BASE + 0x8ul);
	const int fpgaRevision = *fpgaRevisionRegister;
	int * const CacheControlRegister = (int*)CCR;

	printk("SuperH SH4-202 MicroDev board (FPGA rev: 0x%0x, CCR: 0x%0x)\n",
		fpgaRevision, *CacheControlRegister);

	return;
}


