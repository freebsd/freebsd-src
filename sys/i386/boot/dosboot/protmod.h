/*
 *	protmod.h		Protected Mode Utilities
 *
 *	(C) 1994 by Christian Gusenbauer (cg@fimp01.fim.uni-linz.ac.at)
 *	All Rights Reserved.
 * 
 *	Permission to use, copy, modify and distribute this software and its
 *	documentation is hereby granted, provided that both the copyright
 *	notice and this permission notice appear in all copies of the
 *	software, derivative works or modified versions, and any portions
 *	thereof, and that both notices appear in supporting documentation.
 * 
 *	I ALLOW YOU USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION. I DISCLAIM
 *	ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
 *	USE OF THIS SOFTWARE.
 * 
 */
extern struct bootinfo bootinfo;
extern int VCPIboot;

extern int pm_copy(char far *from, unsigned long to, unsigned long count);
/* pm_copy copies "count" bytes from location "from" (valid C pointer) to the
 * address "to" in the high-memory space.
 */

extern void startprog(long hmaddress, long size, long startaddr, long loadflags,
					  long bootdev);
/* startprog switches to protected mode, moves the kernel from hmaddress
 * to 0x100000l and finally starts the kernel.
 */

extern long get_high_memory(long size);
/* get_high_memory allocates size bytes from high memory (>1MB) and returns
 * the address of this area.
 */
