/*
 * gdb-low.S contains the low-level trap handler for the GDB stub.
 *
 * Copyright (C) 1995 Andreas Busse
 */
#include <linux/config.h>
#include <linux/sys.h>

#include <asm/asm.h>
#include <asm/errno.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <asm/gdb-stub.h>

/*
 * [jsun] We reserves about 2x GDB_FR_SIZE in stack.  The lower (addressed)
 * part is used to store registers and passed to exception handler.
 * The upper part is reserved for "call func" feature where gdb client
 * saves some of the regs, setups call frame and passes args.
 *
 * A trace shows about 200 bytes are used to store about half of all regs.
 * The rest should be big enough for frame setup and passing args.
 */

/*
 * The low level trap handler
 */
		.align 	5
		NESTED(trap_low, GDB_FR_SIZE, sp)
 		.set	noat
		.set 	noreorder

		mfc0	k0,CP0_STATUS
		sll	k0,3     		/* extract cu0 bit */
		bltz	k0,1f
		move	k1,sp

		/*
		 * Called from user mode, go somewhere else.
		 */
		lui	k1,%hi(saved_vectors)
		mfc0	k0,CP0_CAUSE
		andi	k0,k0,0x7c
		add	k1,k1,k0
		lw	k0,%lo(saved_vectors)(k1)
		jr	k0
		nop
1:
		move	k0,sp
		subu	sp,k1,GDB_FR_SIZE*2	# see comment above
		sw	k0,GDB_FR_REG29(sp)
		sw	v0,GDB_FR_REG2(sp)

/*
 * First save the CP0 and special registers
 */

		mfc0	v0,CP0_STATUS
		sw	v0,GDB_FR_STATUS(sp)
		mfc0	v0,CP0_CAUSE
		sw	v0,GDB_FR_CAUSE(sp)
		mfc0	v0,CP0_EPC
		sw	v0,GDB_FR_EPC(sp)
		mfc0	v0,CP0_BADVADDR
		sw	v0,GDB_FR_BADVADDR(sp)
		mfhi	v0
		sw	v0,GDB_FR_HI(sp)
		mflo	v0
		sw	v0,GDB_FR_LO(sp)

/*
 * Now the integer registers
 */

		sw	zero,GDB_FR_REG0(sp)		/* I know... */
		sw	$1,GDB_FR_REG1(sp)
		/* v0 already saved */
		sw	v1,GDB_FR_REG3(sp)
		sw	a0,GDB_FR_REG4(sp)
		sw	a1,GDB_FR_REG5(sp)
		sw	a2,GDB_FR_REG6(sp)
		sw	a3,GDB_FR_REG7(sp)
		sw	t0,GDB_FR_REG8(sp)
		sw	t1,GDB_FR_REG9(sp)
		sw	t2,GDB_FR_REG10(sp)
		sw	t3,GDB_FR_REG11(sp)
		sw	t4,GDB_FR_REG12(sp)
		sw	t5,GDB_FR_REG13(sp)
		sw	t6,GDB_FR_REG14(sp)
		sw	t7,GDB_FR_REG15(sp)
		sw	s0,GDB_FR_REG16(sp)
		sw	s1,GDB_FR_REG17(sp)
		sw	s2,GDB_FR_REG18(sp)
		sw	s3,GDB_FR_REG19(sp)
		sw	s4,GDB_FR_REG20(sp)
		sw	s5,GDB_FR_REG21(sp)
		sw	s6,GDB_FR_REG22(sp)
		sw	s7,GDB_FR_REG23(sp)
		sw	t8,GDB_FR_REG24(sp)
		sw	t9,GDB_FR_REG25(sp)
		sw	k0,GDB_FR_REG26(sp)
		sw	k1,GDB_FR_REG27(sp)
		sw	gp,GDB_FR_REG28(sp)
		/* sp already saved */
		sw	fp,GDB_FR_REG30(sp)
		sw	ra,GDB_FR_REG31(sp)

		CLI				/* disable interrupts */

/*
 * Followed by the floating point registers
 */
		mfc0	v0,CP0_STATUS		/* FPU enabled? */
		srl	v0,v0,16
		andi	v0,v0,(ST0_CU1 >> 16)

		beqz	v0,2f			/* disabled, skip */
		 nop

		swc1	$0,GDB_FR_FPR0(sp)
		swc1	$1,GDB_FR_FPR1(sp)
		swc1	$2,GDB_FR_FPR2(sp)
		swc1	$3,GDB_FR_FPR3(sp)
		swc1	$4,GDB_FR_FPR4(sp)
		swc1	$5,GDB_FR_FPR5(sp)
		swc1	$6,GDB_FR_FPR6(sp)
		swc1	$7,GDB_FR_FPR7(sp)
		swc1	$8,GDB_FR_FPR8(sp)
		swc1	$9,GDB_FR_FPR9(sp)
		swc1	$10,GDB_FR_FPR10(sp)
		swc1	$11,GDB_FR_FPR11(sp)
		swc1	$12,GDB_FR_FPR12(sp)
		swc1	$13,GDB_FR_FPR13(sp)
		swc1	$14,GDB_FR_FPR14(sp)
		swc1	$15,GDB_FR_FPR15(sp)
		swc1	$16,GDB_FR_FPR16(sp)
		swc1	$17,GDB_FR_FPR17(sp)
		swc1	$18,GDB_FR_FPR18(sp)
		swc1	$19,GDB_FR_FPR19(sp)
		swc1	$20,GDB_FR_FPR20(sp)
		swc1	$21,GDB_FR_FPR21(sp)
		swc1	$22,GDB_FR_FPR22(sp)
		swc1	$23,GDB_FR_FPR23(sp)
		swc1	$24,GDB_FR_FPR24(sp)
		swc1	$25,GDB_FR_FPR25(sp)
		swc1	$26,GDB_FR_FPR26(sp)
		swc1	$27,GDB_FR_FPR27(sp)
		swc1	$28,GDB_FR_FPR28(sp)
		swc1	$29,GDB_FR_FPR29(sp)
		swc1	$30,GDB_FR_FPR30(sp)
		swc1	$31,GDB_FR_FPR31(sp)

/*
 * FPU control registers
 */

		cfc1	v0,CP1_STATUS
		sw	v0,GDB_FR_FSR(sp)
		cfc1	v0,CP1_REVISION
		sw	v0,GDB_FR_FIR(sp)

/*
 * Current stack frame ptr
 */

2:
		sw	sp,GDB_FR_FRP(sp)

/*
 * CP0 registers (R4000/R4400 unused registers skipped)
 */

		mfc0	v0,CP0_INDEX
		sw	v0,GDB_FR_CP0_INDEX(sp)
		mfc0	v0,CP0_RANDOM
		sw	v0,GDB_FR_CP0_RANDOM(sp)
		mfc0	v0,CP0_ENTRYLO0
		sw	v0,GDB_FR_CP0_ENTRYLO0(sp)
		mfc0	v0,CP0_ENTRYLO1
		sw	v0,GDB_FR_CP0_ENTRYLO1(sp)
		mfc0	v0,CP0_CONTEXT
		sw	v0,GDB_FR_CP0_CONTEXT(sp)
		mfc0	v0,CP0_PAGEMASK
		sw	v0,GDB_FR_CP0_PAGEMASK(sp)
		mfc0	v0,CP0_WIRED
		sw	v0,GDB_FR_CP0_WIRED(sp)
		mfc0	v0,CP0_ENTRYHI
		sw	v0,GDB_FR_CP0_ENTRYHI(sp)
		mfc0	v0,CP0_PRID
		sw	v0,GDB_FR_CP0_PRID(sp)

		.set	at

/*
 * Continue with the higher level handler
 */

		move	a0,sp

		jal	handle_exception
		 nop

/*
 * Restore all writable registers, in reverse order
 */

		.set	noat

		lw	v0,GDB_FR_CP0_ENTRYHI(sp)
		lw	v1,GDB_FR_CP0_WIRED(sp)
		mtc0	v0,CP0_ENTRYHI
		mtc0	v1,CP0_WIRED
		lw	v0,GDB_FR_CP0_PAGEMASK(sp)
		lw	v1,GDB_FR_CP0_ENTRYLO1(sp)
		mtc0	v0,CP0_PAGEMASK
		mtc0	v1,CP0_ENTRYLO1
		lw	v0,GDB_FR_CP0_ENTRYLO0(sp)
		lw	v1,GDB_FR_CP0_INDEX(sp)
		mtc0	v0,CP0_ENTRYLO0
		lw	v0,GDB_FR_CP0_CONTEXT(sp)
		mtc0	v1,CP0_INDEX
		mtc0	v0,CP0_CONTEXT


/*
 * Next, the floating point registers
 */
		mfc0	v0,CP0_STATUS		/* check if the FPU is enabled */
		srl	v0,v0,16
		andi	v0,v0,(ST0_CU1 >> 16)

		beqz	v0,3f			/* disabled, skip */
		 nop

		lwc1	$31,GDB_FR_FPR31(sp)
		lwc1	$30,GDB_FR_FPR30(sp)
		lwc1	$29,GDB_FR_FPR29(sp)
		lwc1	$28,GDB_FR_FPR28(sp)
		lwc1	$27,GDB_FR_FPR27(sp)
		lwc1	$26,GDB_FR_FPR26(sp)
		lwc1	$25,GDB_FR_FPR25(sp)
		lwc1	$24,GDB_FR_FPR24(sp)
		lwc1	$23,GDB_FR_FPR23(sp)
		lwc1	$22,GDB_FR_FPR22(sp)
		lwc1	$21,GDB_FR_FPR21(sp)
		lwc1	$20,GDB_FR_FPR20(sp)
		lwc1	$19,GDB_FR_FPR19(sp)
		lwc1	$18,GDB_FR_FPR18(sp)
		lwc1	$17,GDB_FR_FPR17(sp)
		lwc1	$16,GDB_FR_FPR16(sp)
		lwc1	$15,GDB_FR_FPR15(sp)
		lwc1	$14,GDB_FR_FPR14(sp)
		lwc1	$13,GDB_FR_FPR13(sp)
		lwc1	$12,GDB_FR_FPR12(sp)
		lwc1	$11,GDB_FR_FPR11(sp)
		lwc1	$10,GDB_FR_FPR10(sp)
		lwc1	$9,GDB_FR_FPR9(sp)
		lwc1	$8,GDB_FR_FPR8(sp)
		lwc1	$7,GDB_FR_FPR7(sp)
		lwc1	$6,GDB_FR_FPR6(sp)
		lwc1	$5,GDB_FR_FPR5(sp)
		lwc1	$4,GDB_FR_FPR4(sp)
		lwc1	$3,GDB_FR_FPR3(sp)
		lwc1	$2,GDB_FR_FPR2(sp)
		lwc1	$1,GDB_FR_FPR1(sp)
		lwc1	$0,GDB_FR_FPR0(sp)

/*
 * Now the CP0 and integer registers
 */

3:
		mfc0	t0,CP0_STATUS
		ori	t0,0x1f
		xori	t0,0x1f
		mtc0	t0,CP0_STATUS

		lw	v0,GDB_FR_STATUS(sp)
		lw	v1,GDB_FR_EPC(sp)
		mtc0	v0,CP0_STATUS
		mtc0	v1,CP0_EPC
		lw	v0,GDB_FR_HI(sp)
		lw	v1,GDB_FR_LO(sp)
		mthi	v0
		mtlo	v0
		lw	ra,GDB_FR_REG31(sp)
		lw	fp,GDB_FR_REG30(sp)
		lw	gp,GDB_FR_REG28(sp)
		lw	k1,GDB_FR_REG27(sp)
		lw	k0,GDB_FR_REG26(sp)
		lw	t9,GDB_FR_REG25(sp)
		lw	t8,GDB_FR_REG24(sp)
		lw	s7,GDB_FR_REG23(sp)
		lw	s6,GDB_FR_REG22(sp)
		lw	s5,GDB_FR_REG21(sp)
		lw	s4,GDB_FR_REG20(sp)
		lw	s3,GDB_FR_REG19(sp)
		lw	s2,GDB_FR_REG18(sp)
		lw	s1,GDB_FR_REG17(sp)
		lw	s0,GDB_FR_REG16(sp)
		lw	t7,GDB_FR_REG15(sp)
		lw	t6,GDB_FR_REG14(sp)
		lw	t5,GDB_FR_REG13(sp)
		lw	t4,GDB_FR_REG12(sp)
		lw	t3,GDB_FR_REG11(sp)
		lw	t2,GDB_FR_REG10(sp)
		lw	t1,GDB_FR_REG9(sp)
		lw	t0,GDB_FR_REG8(sp)
		lw	a3,GDB_FR_REG7(sp)
		lw	a2,GDB_FR_REG6(sp)
		lw	a1,GDB_FR_REG5(sp)
		lw	a0,GDB_FR_REG4(sp)
		lw	v1,GDB_FR_REG3(sp)
		lw	v0,GDB_FR_REG2(sp)
		lw	$1,GDB_FR_REG1(sp)
#if defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_TX39XX)
		lw	k0, GDB_FR_EPC(sp)
		lw	sp, GDB_FR_REG29(sp)		/* Deallocate stack */
		jr	k0
		rfe
#else
		lw	sp, GDB_FR_REG29(sp)		/* Deallocate stack */

		.set	mips3
		eret
		.set	mips0
#endif
		.set	at
		.set	reorder
		END(trap_low)

LEAF(kgdb_read_byte)
4:		lb	t0, (a0)
		sb	t0, (a1)
		li	v0, 0
		jr	ra
		.section __ex_table,"a"
		PTR	4b, kgdbfault
		.previous
		END(kgdb_read_byte)

LEAF(kgdb_write_byte)
5:		sb	a0, (a1)
		li	v0, 0
		jr	ra
		.section __ex_table,"a"
		PTR	5b, kgdbfault
		.previous
		END(kgdb_write_byte)

		.type	kgdbfault@function
		.ent	kgdbfault

kgdbfault:	li	v0, -EFAULT
		jr	ra
		.end	kgdbfault
