/* 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc.  All rights reserved.
 */



#include <linux/config.h>
#include <linux/types.h>
#include <asm/bitops.h>
#include <asm/sn/simulator.h>
#include "fpmem.h"

extern void klgraph_init(void);
void sys_fw_init (const char *args, int arglen);

int
fmain(int lid, int bsp) {
	int	nasid, cpu, mynasid, mycpu, bootmaster;

	/*
	 * * Pass the parameter base address to the build_efi_xxx routines.
	 */
#if defined(SGI_SN2)
	build_init(0x3000000000UL | ((long)base_nasid<<38));
#endif

	/*
	 * First lets figure out who we are. This is done from the
	 * LID passed to us.
	 */
	mynasid = (lid>>16)&0xfff;
	mycpu = (lid>>28)&3;

	/*
	 * Determine if THIS cpu is the bootmaster. The <bsp> parameter
	 * is the logical cpu of the bootmaster. Cpus are numbered
	 * low-to-high nasid/lid.
	 */
	GetLogicalCpu(bsp, &nasid, &cpu);
	bootmaster = (mynasid == nasid) && (mycpu == cpu);
	
	/*
	 * Initialize SAL & EFI tables.
	 *   Note: non-bootmaster cpus will return to the slave loop 
	 *   in fpromasm.S. They spin there until they receive an
	 *   external interrupt from the master cpu.
	 */
	if (bootmaster) {
#if 0
		/*
		 * Undef if you need fprom to generate a 1 node klgraph
		 * information .. only works for 1 node for nasid 0.
		 */
		klgraph_init();
#endif
		sys_fw_init(0, 0);
	}

	return (bootmaster);
}



/* Why isnt there a bcopy/memcpy in lib64.a */

void* 
memcpy(void * dest, const void *src, size_t count)
{
	char *s, *se, *d;

	for(d=dest, s=(char*)src, se=s+count; s<se; s++, d++)
		*d = *s;
	return dest;
}
