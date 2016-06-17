/*
 * arch/ppc/boot/simple/head.S
 *
 * Initial board bringup code for many different boards.
 *
 * Author: Tom Rini
 *	   trini@mvista.com
 * Derived from arch/ppc/boot/prep/head.S (Cort Dougan, many others).
 *
 * 2001-2003 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#include <linux/config.h>
#include <asm/processor.h>
#include <asm/cache.h>
#include <asm/ppc_asm.h>

	.text

/*
 *      Begin at some arbitrary location in RAM or Flash
 *	  Initialize core registers
 *	  Configure memory controller (Not executing from RAM)
 *	Move the boot code to the link address (8M)
 *	  Setup C stack
 *	  Initialize UART
 *      Decompress the kernel to 0x0
 *      Jump to the kernel entry
 *
 */

	.globl	start
start:
	bl	start_
#ifdef CONFIG_IBM_OPENBIOS
	/* The IBM OpenBIOS bootroms know that the address of the bootrom
	 * read only structure is 4 bytes after _start.
	 */
	.long	0x62726f6d		# structure ID - "brom"
	.long	0x5f726f00		#              - "_ro\0"
	.long	1			# structure version
	.long	bootrom_cmdline		# address of *bootrom_cmdline
#endif

start_:
#ifdef CONFIG_FORCE
	/* We have some really bad firmware.  We must disable the L1
	 * icache/dcache now or the board won't boot.
	 */
	li	r4,0x0000
	isync
	mtspr	HID0,r4
	sync
	isync
#endif

#if defined(CONFIG_MBX) || defined(CONFIG_RPX6)
	mr	r29,r3	/* On the MBX860, r3 is the board info pointer.
			 * On the RPXSUPER, r3 points to the
			 * NVRAM configuration keys.
			 */
#endif

	mflr	r3	/* Save our actual starting address. */

	/* The following functions we call must not modify r3 or r4.....
	*/
#ifdef CONFIG_6xx
	bl	disable_6xx_mmu
	bl	disable_6xx_l1cache
#if defined(CONFIG_FORCE) || defined(CONFIG_K2) || defined(CONFIG_EV64260)
	bl	_setup_L2CR

	/* If 745x, turn off L3CR as well */
	mfspr	r8,PVR
	srwi	r8,r8,16

	cmpli	cr0,r8,0x8000			/* 7450 */
	cmpli	cr1,r8,0x8001			/* 7455 */
	cmpli	cr2,r8,0x8002			/* 7457 */
	cror	4*cr0+eq,4*cr0+eq,4*cr1+eq	/* Now test if any are true. */
	cror	4*cr0+eq,4*cr0+eq,4*cr2+eq
	beql	_setup_L3CR
#endif
#endif

#ifdef CONFIG_8xx
	mfmsr	r8		/* Turn off interrupts */
	li	r9,0
	ori	r9,r9,MSR_EE
	andc	r8,r8,r9
	mtmsr	r8

	/* We do this because some boot roms don't initialize the
	 * processor correctly. Don't do this if you want to debug
	 * using a BDM device.
	 */
	li	r4,0		/* Zero DER to prevent FRZ */
	mtspr	SPRN_DER,r4
#endif

#ifdef CONFIG_REDWOOD_4
	/* All of this Redwood 4 stuff will soon disappear when the
	 * boot rom is straightened out.
	 */
	mr	r29, r3		/* Easier than changing the other code */
	bl	HdwInit
	mr	r3, r29
#endif

#if defined(CONFIG_MBX) || defined(CONFIG_RPX6)
	mr	r4,r29	/* put the board info pointer where the relocate
			 * routine will find it
			 */
#endif

#ifdef	CONFIG_EV64260
	/* Move 64260's base regs & CS window for external UART */
	bl	ev64260_init
#endif

	/* Get the load address.
	*/
	subi	r3, r3, 4	/* Get the actual IP, not NIP */
	b	relocate

