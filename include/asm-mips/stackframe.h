/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 *  Copyright (C) 1994, 1995, 1996, 2001 Ralf Baechle
 *  Copyright (C) 1994, 1995, 1996 Paul M. Antoine.
 */
#ifndef __ASM_STACKFRAME_H
#define __ASM_STACKFRAME_H

#include <linux/config.h>
#include <asm/addrspace.h>
#include <asm/mipsregs.h>
#include <asm/processor.h>
#include <asm/asm.h>
#include <asm/offset.h>

#define SAVE_AT                                          \
		.set	push;                            \
		.set	noat;                            \
		sw	$1, PT_R1(sp);                   \
		.set	pop

#define SAVE_TEMP                                        \
		mfhi	v1;                              \
		sw	$8, PT_R8(sp);                   \
		sw	$9, PT_R9(sp);                   \
		sw	v1, PT_HI(sp);                   \
		mflo	v1;                              \
		sw	$10,PT_R10(sp);                  \
		sw	$11, PT_R11(sp);                 \
		sw	v1,  PT_LO(sp);                  \
		sw	$12, PT_R12(sp);                 \
		sw	$13, PT_R13(sp);                 \
		sw	$14, PT_R14(sp);                 \
		sw	$15, PT_R15(sp);                 \
		sw	$24, PT_R24(sp)

#define SAVE_STATIC                                      \
		sw	$16, PT_R16(sp);                 \
		sw	$17, PT_R17(sp);                 \
		sw	$18, PT_R18(sp);                 \
		sw	$19, PT_R19(sp);                 \
		sw	$20, PT_R20(sp);                 \
		sw	$21, PT_R21(sp);                 \
		sw	$22, PT_R22(sp);                 \
		sw	$23, PT_R23(sp);                 \
		sw	$30, PT_R30(sp)

#ifdef CONFIG_SMP
#  define GET_SAVED_SP                                   \
                mfc0    k0, CP0_CONTEXT;                 \
                lui     k1, %hi(kernelsp);               \
                srl     k0, k0, 23;                      \
		sll     k0, k0, 2;                       \
                addu    k1, k0;                          \
                lw      k1, %lo(kernelsp)(k1);

#else
#  define GET_SAVED_SP                                   \
		lui	k1, %hi(kernelsp);               \
		lw	k1, %lo(kernelsp)(k1);
#endif

#define SAVE_SOME                                        \
		.set	push;                            \
		.set	reorder;                         \
		mfc0	k0, CP0_STATUS;                  \
		sll	k0, 3;     /* extract cu0 bit */ \
		.set	noreorder;                       \
		bltz	k0, 8f;                          \
		 move	k1, sp;                          \
		.set	reorder;                         \
		/* Called from user mode, new stack. */  \
                GET_SAVED_SP                             \
8:                                                       \
		move	k0, sp;                          \
		subu	sp, k1, PT_SIZE;                 \
		sw	k0, PT_R29(sp);                  \
                sw	$3, PT_R3(sp);                   \
		sw	$0, PT_R0(sp);			 \
		mfc0	v1, CP0_STATUS;                  \
		sw	$2, PT_R2(sp);                   \
		sw	v1, PT_STATUS(sp);               \
		sw	$4, PT_R4(sp);                   \
		mfc0	v1, CP0_CAUSE;                   \
		sw	$5, PT_R5(sp);                   \
		sw	v1, PT_CAUSE(sp);                \
		sw	$6, PT_R6(sp);                   \
		mfc0	v1, CP0_EPC;                     \
		sw	$7, PT_R7(sp);                   \
		sw	v1, PT_EPC(sp);                  \
		sw	$25, PT_R25(sp);                 \
		sw	$28, PT_R28(sp);                 \
		sw	$31, PT_R31(sp);                 \
		ori	$28, sp, 0x1fff;                 \
		xori	$28, 0x1fff;                     \
		.set	pop

#define SAVE_ALL                                         \
		SAVE_SOME;                               \
		SAVE_AT;                                 \
		SAVE_TEMP;                               \
		SAVE_STATIC

#define RESTORE_AT                                       \
		.set	push;                            \
		.set	noat;                            \
		lw	$1,  PT_R1(sp);                  \
		.set	pop;

#define RESTORE_TEMP                                     \
		lw	$24, PT_LO(sp);                  \
		lw	$8, PT_R8(sp);                   \
		lw	$9, PT_R9(sp);                   \
		mtlo	$24;                             \
		lw	$24, PT_HI(sp);                  \
		lw	$10,PT_R10(sp);                  \
		lw	$11, PT_R11(sp);                 \
		mthi	$24;                             \
		lw	$12, PT_R12(sp);                 \
		lw	$13, PT_R13(sp);                 \
		lw	$14, PT_R14(sp);                 \
		lw	$15, PT_R15(sp);                 \
		lw	$24, PT_R24(sp)

#define RESTORE_STATIC                                   \
		lw	$16, PT_R16(sp);                 \
		lw	$17, PT_R17(sp);                 \
		lw	$18, PT_R18(sp);                 \
		lw	$19, PT_R19(sp);                 \
		lw	$20, PT_R20(sp);                 \
		lw	$21, PT_R21(sp);                 \
		lw	$22, PT_R22(sp);                 \
		lw	$23, PT_R23(sp);                 \
		lw	$30, PT_R30(sp)

#if defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_TX39XX)

#define RESTORE_SOME                                     \
		.set	push;                            \
		.set	reorder;                         \
		mfc0	t0, CP0_STATUS;                  \
		.set	pop;                             \
		ori	t0, 0x1f;                        \
		xori	t0, 0x1f;                        \
		mtc0	t0, CP0_STATUS;                  \
		li	v1, 0xff00;                      \
		and	t0, v1;				 \
		lw	v0, PT_STATUS(sp);               \
		nor	v1, $0, v1;			 \
		and	v0, v1;				 \
		or	v0, t0;				 \
		mtc0	v0, CP0_STATUS;                  \
		lw	$31, PT_R31(sp);                 \
		lw	$28, PT_R28(sp);                 \
		lw	$25, PT_R25(sp);                 \
		lw	$7,  PT_R7(sp);                  \
		lw	$6,  PT_R6(sp);                  \
		lw	$5,  PT_R5(sp);                  \
		lw	$4,  PT_R4(sp);                  \
		lw	$3,  PT_R3(sp);                  \
		lw	$2,  PT_R2(sp)

#define RESTORE_SP_AND_RET                               \
		.set	push;				 \
		.set	noreorder;			 \
		lw	k0, PT_EPC(sp);                  \
		lw	sp,  PT_R29(sp);                 \
		jr	k0;                              \
		 rfe;					 \
		.set	pop

#else

#define RESTORE_SOME                                     \
		.set	push;                            \
		.set	reorder;                         \
		mfc0	t0, CP0_STATUS;                  \
		.set	pop;                             \
		ori	t0, 0x1f;                        \
		xori	t0, 0x1f;                        \
		mtc0	t0, CP0_STATUS;                  \
		li	v1, 0xff00;                      \
		and	t0, v1;				 \
		lw	v0, PT_STATUS(sp);               \
		nor	v1, $0, v1;			 \
		and	v0, v1;				 \
		or	v0, t0;				 \
		mtc0	v0, CP0_STATUS;                  \
		lw	v1, PT_EPC(sp);                  \
		mtc0	v1, CP0_EPC;                     \
		lw	$31, PT_R31(sp);                 \
		lw	$28, PT_R28(sp);                 \
		lw	$25, PT_R25(sp);                 \
		lw	$7,  PT_R7(sp);                  \
		lw	$6,  PT_R6(sp);                  \
		lw	$5,  PT_R5(sp);                  \
		lw	$4,  PT_R4(sp);                  \
		lw	$3,  PT_R3(sp);                  \
		lw	$2,  PT_R2(sp)

#define RESTORE_SP_AND_RET                               \
		lw	sp,  PT_R29(sp);                 \
		.set	mips3;				 \
		eret;					 \
		.set	mips0

#endif

#define RESTORE_SP                                       \
		lw	sp,  PT_R29(sp);                 \

#define RESTORE_ALL                                      \
		RESTORE_SOME;                            \
		RESTORE_AT;                              \
		RESTORE_TEMP;                            \
		RESTORE_STATIC;                          \
		RESTORE_SP

#define RESTORE_ALL_AND_RET                              \
		RESTORE_SOME;                            \
		RESTORE_AT;                              \
		RESTORE_TEMP;                            \
		RESTORE_STATIC;                          \
		RESTORE_SP_AND_RET


/*
 * Move to kernel mode and disable interrupts.
 * Set cp0 enable bit as sign that we're running on the kernel stack
 */
#define CLI                                             \
		mfc0	t0,CP0_STATUS;                  \
		li	t1,ST0_CU0|0x1f;                \
		or	t0,t1;                          \
		xori	t0,0x1f;                        \
		mtc0	t0,CP0_STATUS

/*
 * Move to kernel mode and enable interrupts.
 * Set cp0 enable bit as sign that we're running on the kernel stack
 */
#define STI                                             \
		mfc0	t0,CP0_STATUS;                  \
		li	t1,ST0_CU0|0x1f;                \
		or	t0,t1;                          \
		xori	t0,0x1e;                        \
		mtc0	t0,CP0_STATUS

/*
 * Just move to kernel mode and leave interrupts as they are.
 * Set cp0 enable bit as sign that we're running on the kernel stack
 */
#define KMODE                                           \
		mfc0	t0,CP0_STATUS;                  \
		li	t1,ST0_CU0|0x1e;                \
		or	t0,t1;                          \
		xori	t0,0x1e;                        \
		mtc0	t0,CP0_STATUS

#endif /* __ASM_STACKFRAME_H */
