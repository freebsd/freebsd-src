/* 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/pci.h>
#include <linux/module.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/sn/simulator.h>
#include <asm/sn/pda.h>
#include <asm/sn/sn_cpuid.h>

/**
 * sn_io_addr - convert an in/out port to an i/o address
 * @port: port to convert
 *
 * Legacy in/out instructions are converted to ld/st instructions
 * on IA64.  This routine will convert a port number into a valid 
 * SN i/o address.  Used by sn_in*() and sn_out*().
 */
void *
sn_io_addr(unsigned long port)
{
	if (!IS_RUNNING_ON_SIMULATOR()) {
		/* On sn2, legacy I/O ports don't point at anything */
		if (port < 64*1024)
			return 0;
		return( (void *)  (port | __IA64_UNCACHED_OFFSET));
	} else {
		/* but the simulator uses them... */
		unsigned long addr;
 
		/*
 		 * word align port, but need more than 10 bits
 		 * for accessing registers in bedrock local block
 		 * (so we don't do port&0xfff)
 		 */
		addr = 0xc0000087cc000000 | ((port >> 2) << 12);
		if ((port >= 0x1f0 && port <= 0x1f7) || port == 0x3f6 || port == 0x3f7)
			addr |= port;
		return(void *) addr;
	}
}

EXPORT_SYMBOL(sn_io_addr);

/**
 * sn_mmiob - I/O space memory barrier
 *
 * Acts as a memory mapped I/O barrier for platforms that queue writes to 
 * I/O space.  This ensures that subsequent writes to I/O space arrive after
 * all previous writes.  For most ia64 platforms, this is a simple
 * 'mf.a' instruction.  For other platforms, mmiob() may have to read
 * a chipset register to ensure ordering.
 *
 * On SN2, we wait for the PIO_WRITE_STATUS SHub register to clear.
 * See PV 871084 for details about the WAR about zero value.
 *
 */
void
sn_mmiob (void)
{
	while ((((volatile unsigned long) (*pda.pio_write_status_addr)) & SH_PIO_WRITE_STATUS_0_PENDING_WRITE_COUNT_MASK) != 
				SH_PIO_WRITE_STATUS_0_PENDING_WRITE_COUNT_MASK)
		udelay(1);
}
