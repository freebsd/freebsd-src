/*
 * balo_supp.S: BAget Loader supplement
 *
 * Copyright (C) 1998 Gleb Raiko & Vladimir Roganov
 */
#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <asm/mipsregs.h>
#include <asm/addrspace.h>

	.text
	.set    mips1

	/* General exception vector. */
NESTED(except_vec3_generic, 0, sp)
	.set    noat
	la	k0, except_vec3_generic_code
	jr	k0
END(except_vec3_generic)

NESTED(except_vec3_generic_code, 0, sp)
	SAVE_ALL
	mfc0	k1, CP0_CAUSE
	la	k0, int_cause
	sw      k1, (k0)

        mfc0    k1, CP0_EPC
        la      k0, epc
        sw      k1, (k0)

        mfc0    k1, CP0_BADVADDR
        la      k0, badvaddr
        sw      k1, (k0)

	la	k0, int_handler
        .set    noreorder
	jal	k0
        .set    reorder
	move	a0, sp

        RESTORE_ALL_AND_RET
END(except_vec3_generic_code)

        .align  5
NESTED(flush_cache_low, PT_SIZE, sp)
        .set    at
        .set    macro
        .set    noreorder

        move    t1, a0  # ISIZE
        move    t2, a1  # DSIZE

        mfc0    t3, CP0_STATUS       # Save the status register.
        mtc0    zero, CP0_STATUS     # Disable interrupts.
        la      v0, 1f
        or      v0, KSEG1            # Run uncached.
        j       v0
        nop
/*
 * Flush the instruction cache.
 */
1:
        li      v0, ST0_DE | ST0_CE
        mtc0    v0, CP0_STATUS       # Isolate and swap caches.
        li      t0, KSEG1
        subu    t0, t0, t1
        li      t1, KSEG1
        la      v0, 1f                          # Run cached
        j       v0
        nop
1:
        addu    t0, t0, 64
        sb      zero, -64(t0)
        sb      zero, -60(t0)
        sb      zero, -56(t0)
        sb      zero, -52(t0)
        sb      zero, -48(t0)
        sb      zero, -44(t0)
        sb      zero, -40(t0)
        sb      zero, -36(t0)
        sb      zero, -32(t0)
        sb      zero, -28(t0)
        sb      zero, -24(t0)
        sb      zero, -20(t0)
        sb      zero, -16(t0)
        sb      zero, -12(t0)
        sb      zero, -8(t0)
        bne     t0, t1, 1b
        sb      zero, -4(t0)

        la      v0, 1f
        or      v0, KSEG1
        j       v0                              # Run uncached
        nop
/*
 * Flush the data cache.
 */
1:
        li      v0, ST0_DE
        mtc0    v0, CP0_STATUS       # Isolate and swap back caches
        li      t0, KSEG1
        subu    t0, t0, t2
        la      v0, 1f
        j       v0                              # Back to cached mode
        nop
1:
        addu    t0, t0, 64
        sb      zero, -64(t0)
        sb      zero, -60(t0)
        sb      zero, -56(t0)
        sb      zero, -52(t0)
        sb      zero, -48(t0)
        sb      zero, -44(t0)
        sb      zero, -40(t0)
        sb      zero, -36(t0)
        sb      zero, -32(t0)
        sb      zero, -28(t0)
        sb      zero, -24(t0)
        sb      zero, -20(t0)
        sb      zero, -16(t0)
        sb      zero, -12(t0)
        sb      zero, -8(t0)
        bne     t0, t1, 1b
        sb      zero, -4(t0)

        nop                                     # Insure isolated stores
        nop                                     #   out of pipe.
        nop
        nop
        mtc0    t3, CP0_STATUS                 # Restore status reg.
        nop                                    # Insure cache unisolated.
        nop
        nop
        nop
        j       ra
        nop
END(flush_cache_low)

/* To satisfy macros only */
EXPORT(kernelsp)
	PTR	0x80001000
