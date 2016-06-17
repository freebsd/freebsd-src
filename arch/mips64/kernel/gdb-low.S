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
		sd	k0,GDB_FR_REG29(sp)
		sd	v0,GDB_FR_REG2(sp)

/*
 * First save the CP0 and special registers
 */

		mfc0	v0,CP0_STATUS
		sd	v0,GDB_FR_STATUS(sp)
		mfc0	v0,CP0_CAUSE
		sd	v0,GDB_FR_CAUSE(sp)
		dmfc0	v0,CP0_EPC
		sd	v0,GDB_FR_EPC(sp)
		dmfc0	v0,CP0_BADVADDR
		sd	v0,GDB_FR_BADVADDR(sp)
		mfhi	v0
		sd	v0,GDB_FR_HI(sp)
		mflo	v0
		sd	v0,GDB_FR_LO(sp)

/*
 * Now the integer registers
 */

		sd	zero,GDB_FR_REG0(sp)		/* I know... */
		sd	$1,GDB_FR_REG1(sp)
		/* v0 already saved */
		sd	v1,GDB_FR_REG3(sp)
		sd	a0,GDB_FR_REG4(sp)
		sd	a1,GDB_FR_REG5(sp)
		sd	a2,GDB_FR_REG6(sp)
		sd	a3,GDB_FR_REG7(sp)
		sd	a4,GDB_FR_REG8(sp)
		sd	a5,GDB_FR_REG9(sp)
		sd	a6,GDB_FR_REG10(sp)
		sd	a7,GDB_FR_REG11(sp)
		sd	t0,GDB_FR_REG12(sp)
		sd	t1,GDB_FR_REG13(sp)
		sd	t2,GDB_FR_REG14(sp)
		sd	t3,GDB_FR_REG15(sp)
		sd	s0,GDB_FR_REG16(sp)
		sd	s1,GDB_FR_REG17(sp)
		sd	s2,GDB_FR_REG18(sp)
		sd	s3,GDB_FR_REG19(sp)
		sd	s4,GDB_FR_REG20(sp)
		sd	s5,GDB_FR_REG21(sp)
		sd	s6,GDB_FR_REG22(sp)
		sd	s7,GDB_FR_REG23(sp)
		sd	t8,GDB_FR_REG24(sp)
		sd	t9,GDB_FR_REG25(sp)
		sd	k0,GDB_FR_REG26(sp)
		sd	k1,GDB_FR_REG27(sp)
		sd	gp,GDB_FR_REG28(sp)
		/* sp already saved */
		sd	fp,GDB_FR_REG30(sp)
		sd	ra,GDB_FR_REG31(sp)

		CLI				/* disable interrupts */

/*
 * Followed by the floating point registers
 */
		mfc0	v0,CP0_STATUS		/* FPU enabled? */
		srl	v0,v0,16
		andi	v0,v0,(ST0_CU1 >> 16)

		beqz	v0,2f			/* disabled, skip */
		 nop

		sdc1	$0,GDB_FR_FPR0(sp)
		sdc1	$1,GDB_FR_FPR1(sp)
		sdc1	$2,GDB_FR_FPR2(sp)
		sdc1	$3,GDB_FR_FPR3(sp)
		sdc1	$4,GDB_FR_FPR4(sp)
		sdc1	$5,GDB_FR_FPR5(sp)
		sdc1	$6,GDB_FR_FPR6(sp)
		sdc1	$7,GDB_FR_FPR7(sp)
		sdc1	$8,GDB_FR_FPR8(sp)
		sdc1	$9,GDB_FR_FPR9(sp)
		sdc1	$10,GDB_FR_FPR10(sp)
		sdc1	$11,GDB_FR_FPR11(sp)
		sdc1	$12,GDB_FR_FPR12(sp)
		sdc1	$13,GDB_FR_FPR13(sp)
		sdc1	$14,GDB_FR_FPR14(sp)
		sdc1	$15,GDB_FR_FPR15(sp)
		sdc1	$16,GDB_FR_FPR16(sp)
		sdc1	$17,GDB_FR_FPR17(sp)
		sdc1	$18,GDB_FR_FPR18(sp)
		sdc1	$19,GDB_FR_FPR19(sp)
		sdc1	$20,GDB_FR_FPR20(sp)
		sdc1	$21,GDB_FR_FPR21(sp)
		sdc1	$22,GDB_FR_FPR22(sp)
		sdc1	$23,GDB_FR_FPR23(sp)
		sdc1	$24,GDB_FR_FPR24(sp)
		sdc1	$25,GDB_FR_FPR25(sp)
		sdc1	$26,GDB_FR_FPR26(sp)
		sdc1	$27,GDB_FR_FPR27(sp)
		sdc1	$28,GDB_FR_FPR28(sp)
		sdc1	$29,GDB_FR_FPR29(sp)
		sdc1	$30,GDB_FR_FPR30(sp)
		sdc1	$31,GDB_FR_FPR31(sp)

/*
 * FPU control registers
 */

		cfc1	v0,CP1_STATUS
		sd	v0,GDB_FR_FSR(sp)
		cfc1	v0,CP1_REVISION
		sd	v0,GDB_FR_FIR(sp)

/*
 * Current stack frame ptr
 */

2:
		sd	sp,GDB_FR_FRP(sp)

/*
 * CP0 registers (R4000/R4400 unused registers skipped)
 */

		mfc0	v0,CP0_INDEX
		sd	v0,GDB_FR_CP0_INDEX(sp)
		mfc0	v0,CP0_RANDOM
		sd	v0,GDB_FR_CP0_RANDOM(sp)
		dmfc0	v0,CP0_ENTRYLO0
		sd	v0,GDB_FR_CP0_ENTRYLO0(sp)
		dmfc0	v0,CP0_ENTRYLO1
		sd	v0,GDB_FR_CP0_ENTRYLO1(sp)
		dmfc0	v0,CP0_CONTEXT
		sd	v0,GDB_FR_CP0_CONTEXT(sp)
		mfc0	v0,CP0_PAGEMASK
		sd	v0,GDB_FR_CP0_PAGEMASK(sp)
		mfc0	v0,CP0_WIRED
		sd	v0,GDB_FR_CP0_WIRED(sp)
		dmfc0	v0,CP0_ENTRYHI
		sd	v0,GDB_FR_CP0_ENTRYHI(sp)
		mfc0	v0,CP0_PRID
		sd	v0,GDB_FR_CP0_PRID(sp)

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

		ld	v0,GDB_FR_CP0_ENTRYHI(sp)
		ld	v1,GDB_FR_CP0_WIRED(sp)
		dmtc0	v0,CP0_ENTRYHI
		mtc0	v1,CP0_WIRED
		ld	v0,GDB_FR_CP0_PAGEMASK(sp)
		ld	v1,GDB_FR_CP0_ENTRYLO1(sp)
		mtc0	v0,CP0_PAGEMASK
		dmtc0	v1,CP0_ENTRYLO1
		ld	v0,GDB_FR_CP0_ENTRYLO0(sp)
		ld	v1,GDB_FR_CP0_INDEX(sp)
		dmtc0	v0,CP0_ENTRYLO0
		ld	v0,GDB_FR_CP0_CONTEXT(sp)
		mtc0	v1,CP0_INDEX
		dmtc0	v0,CP0_CONTEXT


/*
 * Next, the floating point registers
 */
		mfc0	v0,CP0_STATUS		/* check if the FPU is enabled */
		srl	v0,v0,16
		andi	v0,v0,(ST0_CU1 >> 16)

		beqz	v0,3f			/* disabled, skip */
		 nop

		ldc1	$31,GDB_FR_FPR31(sp)
		ldc1	$30,GDB_FR_FPR30(sp)
		ldc1	$29,GDB_FR_FPR29(sp)
		ldc1	$28,GDB_FR_FPR28(sp)
		ldc1	$27,GDB_FR_FPR27(sp)
		ldc1	$26,GDB_FR_FPR26(sp)
		ldc1	$25,GDB_FR_FPR25(sp)
		ldc1	$24,GDB_FR_FPR24(sp)
		ldc1	$23,GDB_FR_FPR23(sp)
		ldc1	$22,GDB_FR_FPR22(sp)
		ldc1	$21,GDB_FR_FPR21(sp)
		ldc1	$20,GDB_FR_FPR20(sp)
		ldc1	$19,GDB_FR_FPR19(sp)
		ldc1	$18,GDB_FR_FPR18(sp)
		ldc1	$17,GDB_FR_FPR17(sp)
		ldc1	$16,GDB_FR_FPR16(sp)
		ldc1	$15,GDB_FR_FPR15(sp)
		ldc1	$14,GDB_FR_FPR14(sp)
		ldc1	$13,GDB_FR_FPR13(sp)
		ldc1	$12,GDB_FR_FPR12(sp)
		ldc1	$11,GDB_FR_FPR11(sp)
		ldc1	$10,GDB_FR_FPR10(sp)
		ldc1	$9,GDB_FR_FPR9(sp)
		ldc1	$8,GDB_FR_FPR8(sp)
		ldc1	$7,GDB_FR_FPR7(sp)
		ldc1	$6,GDB_FR_FPR6(sp)
		ldc1	$5,GDB_FR_FPR5(sp)
		ldc1	$4,GDB_FR_FPR4(sp)
		ldc1	$3,GDB_FR_FPR3(sp)
		ldc1	$2,GDB_FR_FPR2(sp)
		ldc1	$1,GDB_FR_FPR1(sp)
		ldc1	$0,GDB_FR_FPR0(sp)

/*
 * Now the CP0 and integer registers
 */

3:
		mfc0	t0,CP0_STATUS
		ori	t0,0x1f
		xori	t0,0x1f
		mtc0	t0,CP0_STATUS

		ld	v0,GDB_FR_STATUS(sp)
		ld	v1,GDB_FR_EPC(sp)
		mtc0	v0,CP0_STATUS
		dmtc0	v1,CP0_EPC
		ld	v0,GDB_FR_HI(sp)
		ld	v1,GDB_FR_LO(sp)
		mthi	v0
		mtlo	v0
		ld	ra,GDB_FR_REG31(sp)
		ld	fp,GDB_FR_REG30(sp)
		ld	gp,GDB_FR_REG28(sp)
		ld	k1,GDB_FR_REG27(sp)
		ld	k0,GDB_FR_REG26(sp)
		ld	t9,GDB_FR_REG25(sp)
		ld	t8,GDB_FR_REG24(sp)
		ld	s7,GDB_FR_REG23(sp)
		ld	s6,GDB_FR_REG22(sp)
		ld	s5,GDB_FR_REG21(sp)
		ld	s4,GDB_FR_REG20(sp)
		ld	s3,GDB_FR_REG19(sp)
		ld	s2,GDB_FR_REG18(sp)
		ld	s1,GDB_FR_REG17(sp)
		ld	s0,GDB_FR_REG16(sp)
		ld	t3,GDB_FR_REG15(sp)
		ld	t2,GDB_FR_REG14(sp)
		ld	t1,GDB_FR_REG13(sp)
		ld	t0,GDB_FR_REG12(sp)
		ld	a7,GDB_FR_REG11(sp)
		ld	a6,GDB_FR_REG10(sp)
		ld	a5,GDB_FR_REG9(sp)
		ld	a4,GDB_FR_REG8(sp)
		ld	a3,GDB_FR_REG7(sp)
		ld	a2,GDB_FR_REG6(sp)
		ld	a1,GDB_FR_REG5(sp)
		ld	a0,GDB_FR_REG4(sp)
		ld	v1,GDB_FR_REG3(sp)
		ld	v0,GDB_FR_REG2(sp)
		ld	$1,GDB_FR_REG1(sp)
#if defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_TX39XX)
		ld	k0, GDB_FR_EPC(sp)
		ld	sp, GDB_FR_REG29(sp)		/* Deallocate stack */
		jr	k0
		rfe
#else
		ld	sp, GDB_FR_REG29(sp)		/* Deallocate stack */

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
