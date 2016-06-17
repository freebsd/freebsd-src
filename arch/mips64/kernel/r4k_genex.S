/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 1999 by Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics
 * Copyright (C) 2002  Maciej W. Rozycki
 *
 * Low level exception handling
 */
#include <linux/init.h>
#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/fpregdef.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>
#include <asm/exception.h>
#include <asm/cacheops.h>
#include <asm/war.h>

	BUILD_HANDLER adel ade ade silent		/* #4  */
	BUILD_HANDLER ades ade ade silent		/* #5  */
	BUILD_HANDLER ibe be cli silent			/* #6  */
	BUILD_HANDLER dbe be cli silent			/* #7  */
	BUILD_HANDLER bp bp sti silent			/* #9  */
	BUILD_HANDLER ri ri sti silent			/* #10 */
	BUILD_HANDLER cpu cpu sti silent		/* #11 */
	BUILD_HANDLER ov ov sti silent			/* #12 */
	BUILD_HANDLER tr tr sti silent			/* #13 */
	BUILD_HANDLER fpe fpe fpe silent		/* #15 */
	BUILD_HANDLER mdmx mdmx sti silent		/* #22 */
	BUILD_HANDLER watch watch sti verbose		/* #23 */
	BUILD_HANDLER mcheck mcheck cli verbose		/* #24 */
	BUILD_HANDLER reserved reserved sti verbose	/* others */


	__INIT

/* A temporary overflow handler used by check_daddi(). */

	BUILD_HANDLER  daddi_ov daddi_ov none silent	/* #12 */


/* General exception handler for CPUs with virtual coherency exception.
 *
 * Be careful when changing this, it has to be at most 256 (as a special
 * exception) bytes to fit into space reserved for the exception handler.
 */
	.set	push
	.set	noat
NESTED(except_vec3_r4000, 0, sp)
	mfc0	k1, CP0_CAUSE
	li	k0, 31<<2
	andi	k1, k1, 0x7c
	.set	push
	.set	noreorder
	.set	nomacro
	beq	k1, k0, handle_vced
	 li	k0, 14<<2
	beq	k1, k0, handle_vcei
	 dsll	k1, k1, 1
	.set	pop
	ld	k0, exception_handlers(k1)
	jr	k0

/*
 * Big shit, we now may have two dirty primary cache lines for the same
 * physical address.  We can savely invalidate the line pointed to by
 * c0_badvaddr because after return from this exception handler the load /
 * store will be re-executed.
 */
handle_vced:
	dmfc0	k0, CP0_BADVADDR
	li	k1, -4					# Is this ...
	and	k0, k1					# ... really needed?
	mtc0	zero, CP0_TAGLO
	cache	Index_Store_Tag_D,(k0)
	cache	Hit_Writeback_Inv_SD,(k0)
	dla	k0, vced_count
	lw	k1, (k0)
	addiu	k1, 1
	sw	k1, (k0)
	eret

handle_vcei:
	dmfc0	k0, CP0_BADVADDR
	cache	Hit_Writeback_Inv_SD,(k0)		# also cleans pi
	dla	k0, vcei_count
	lw	k1, (k0)
	addiu	k1, 1
	sw	k1, (k0)
	eret
END(except_vec3_r4000)
	.set	pop


/* General exception vector for all other CPUs.
 *
 * Be careful when changing this, it has to be at most 128 bytes
 * to fit into space reserved for the exception handler.
 */
	.set	push
	.set	noat
NESTED(except_vec3_generic, 0, sp)
#if R5432_CP0_INTERRUPT_WAR
	mfc0    k0, CP0_INDEX
#endif
	mfc0	k1, CP0_CAUSE
	andi	k1, k1, 0x7c
	dsll	k1, k1, 1
	ld	k0, exception_handlers(k1)
	jr	k0
END(except_vec3_generic)
	.set	pop


/*
 * Special interrupt vector for MIPS64 ISA & embedded MIPS processors.
 * This is a dedicated interrupt exception vector which reduces the
 * interrupt processing overhead.  The jump instruction will be replaced
 * at the initialization time.
 *
 * Be careful when changing this, it has to be at most 128 bytes
 * to fit into space reserved for the exception handler.
 */
NESTED(except_vec4, 0, sp)
1:	j	1b			/* Dummy, will be replaced */
END(except_vec4)

	__FINIT
