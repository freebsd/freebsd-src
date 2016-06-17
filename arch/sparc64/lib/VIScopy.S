/* $Id: VIScopy.S,v 1.26 2001/09/27 04:36:24 kanoj Exp $
 * VIScopy.S: High speed copy operations utilizing the UltraSparc
 *            Visual Instruction Set.
 *
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1996, 1997, 1998, 1999 Jakub Jelinek (jj@ultra.linux.cz)
 */

#include "VIS.h"

	/* VIS code can be used for numerous copy/set operation variants.
	 * It can be made to work in the kernel, one single instance,
	 * for all of memcpy, copy_to_user, and copy_from_user by setting
	 * the ASI src/dest globals correctly.  Furthermore it can
	 * be used for kernel-->kernel page copies as well, a hook label
	 * is put in here just for this purpose.
	 *
	 * For userland, compiling this without __KERNEL__ defined makes
	 * it work just fine as a generic libc bcopy and memcpy.
	 * If for userland it is compiled with a 32bit gcc (but you need
	 * -Wa,-Av9a for as), the code will just rely on lower 32bits of
	 * IEU registers, if you compile it with 64bit gcc (ie. define
	 * __sparc_v9__), the code will use full 64bit.
	 */
	 
#ifdef __KERNEL__

#include <asm/visasm.h>
#include <asm/asm_offsets.h>

#define FPU_CLEAN_RETL								\
	ldub		[%g6 + AOFF_task_thread + AOFF_thread_current_ds], %o1;	\
	VISExit									\
	clr		%o0;							\
	retl;									\
	 wr		%o1, %g0, %asi;
#define FPU_RETL								\
	ldub		[%g6 + AOFF_task_thread + AOFF_thread_current_ds], %o1;	\
	VISExit									\
	clr		%o0;							\
	retl;									\
	 wr		%o1, %g0, %asi;
#define NORMAL_RETL								\
	ldub		[%g6 + AOFF_task_thread + AOFF_thread_current_ds], %o1;	\
	clr		%o0;							\
	retl;									\
	 wr		%o1, %g0, %asi;
#define EX(x,y,a,b) 				\
98: 	x,y;					\
	.section .fixup;			\
	.align	4;				\
99:	ba	VIScopyfixup_ret;		\
	 a, b, %o1;				\
	.section __ex_table;			\
	.align	4;				\
	.word	98b, 99b;			\
	.text;					\
	.align	4;
#define EX2(x,y,c,d,e,a,b) 			\
98: 	x,y;					\
	.section .fixup;			\
	.align	4;				\
99:	c, d, e;				\
	ba	VIScopyfixup_ret;		\
	 a, b, %o1;				\
	.section __ex_table;			\
	.align	4;				\
	.word	98b, 99b;			\
	.text;					\
	.align	4;
#define EXO2(x,y) 				\
98: 	x,y;					\
	.section __ex_table;			\
	.align	4;				\
	.word	98b, VIScopyfixup_reto2;	\
	.text;					\
	.align	4;
#define EXVISN(x,y,n) 				\
98: 	x,y;					\
	.section __ex_table;			\
	.align	4;				\
	.word	98b, VIScopyfixup_vis##n;	\
	.text;					\
	.align	4;
#define EXT(start,end,handler) 			\
	.section __ex_table;			\
	.align	4;				\
	.word	start, 0, end, handler;		\
	.text;					\
	.align	4;
#else
#ifdef REGS_64BIT
#define FPU_CLEAN_RETL				\
	retl;					\
	 mov	%g6, %o0;
#define FPU_RETL				\
	retl;					\
	 mov	%g6, %o0;
#else
#define FPU_CLEAN_RETL				\
	wr	%g0, FPRS_FEF, %fprs;		\
	retl;					\
	 mov	%g6, %o0;
#define FPU_RETL				\
	wr	%g0, FPRS_FEF, %fprs;		\
	retl;					\
	 mov	%g6, %o0;
#endif
#define NORMAL_RETL	\
	retl;		\
	 mov	%g6, %o0;
#define EX(x,y,a,b)		x,y
#define EX2(x,y,c,d,e,a,b)	x,y
#define EXO2(x,y)		x,y
#define EXVISN(x,y,n)		x,y
#define EXT(a,b,c)
#endif
#define EXVIS(x,y) EXVISN(x,y,0)
#define EXVIS1(x,y) EXVISN(x,y,1)
#define EXVIS2(x,y) EXVISN(x,y,2)
#define EXVIS3(x,y) EXVISN(x,y,3)
#define EXVIS4(x,y) EXVISN(x,y,4)

#define FREG_FROB(f1, f2, f3, f4, f5, f6, f7, f8, f9)		\
	faligndata		%f1, %f2, %f48;			\
	faligndata		%f2, %f3, %f50;			\
	faligndata		%f3, %f4, %f52;			\
	faligndata		%f4, %f5, %f54;			\
	faligndata		%f5, %f6, %f56;			\
	faligndata		%f6, %f7, %f58;			\
	faligndata		%f7, %f8, %f60;			\
	faligndata		%f8, %f9, %f62;

#define MAIN_LOOP_CHUNK(src, dest, fdest, fsrc, len, jmptgt)	\
	EXVIS(LDBLK		[%src] ASIBLK, %fdest);		\
	ASI_SETDST_BLK						\
	EXVIS(STBLK		%fsrc, [%dest] ASIBLK);		\
	add			%src, 0x40, %src;		\
	subcc			%len, 0x40, %len;		\
	be,pn			%xcc, jmptgt;			\
	 add			%dest, 0x40, %dest;		\
	ASI_SETSRC_BLK

#define LOOP_CHUNK1(src, dest, len, branch_dest)		\
	MAIN_LOOP_CHUNK(src, dest, f0,  f48, len, branch_dest)
#define LOOP_CHUNK2(src, dest, len, branch_dest)		\
	MAIN_LOOP_CHUNK(src, dest, f16, f48, len, branch_dest)
#define LOOP_CHUNK3(src, dest, len, branch_dest)		\
	MAIN_LOOP_CHUNK(src, dest, f32, f48, len, branch_dest)

#define STORE_SYNC(dest, fsrc)					\
	EXVIS(STBLK		%fsrc, [%dest] ASIBLK);		\
	add			%dest, 0x40, %dest;

#ifdef __KERNEL__
#define STORE_JUMP(dest, fsrc, target)				\
	srl			asi_dest, 3, %g5;		\
	EXVIS2(STBLK		%fsrc, [%dest] ASIBLK);		\
	xor		       asi_dest, ASI_BLK_XOR1, asi_dest;\
	add			%dest, 0x40, %dest;		\
	xor			asi_dest, %g5, asi_dest;	\
	ba,pt			%xcc, target;
#else
#define STORE_JUMP(dest, fsrc, target)				\
	EXVIS2(STBLK		%fsrc, [%dest] ASIBLK);		\
	add			%dest, 0x40, %dest;		\
	ba,pt			%xcc, target;
#endif

#ifndef __KERNEL__
#define VISLOOP_PAD nop; nop; nop; nop; \
		    nop; nop; nop; nop; \
		    nop; nop; nop; nop; \
		    nop; nop; nop;
#else
#define VISLOOP_PAD
#endif

#define FINISH_VISCHUNK(dest, f0, f1, left)			\
	ASI_SETDST_NOBLK					\
	subcc			%left, 8, %left;		\
	bl,pn			%xcc, vis_out;			\
	 faligndata		%f0, %f1, %f48;			\
	EXVIS3(STDF		%f48, [%dest] ASINORMAL);	\
	add			%dest, 8, %dest;

#define UNEVEN_VISCHUNK_LAST(dest, f0, f1, left)		\
	subcc			%left, 8, %left;		\
	bl,pn			%xcc, vis_out;			\
	 fsrc1			%f0, %f1;
#define UNEVEN_VISCHUNK(dest, f0, f1, left)			\
	UNEVEN_VISCHUNK_LAST(dest, f0, f1, left)		\
	ba,a,pt			%xcc, vis_out_slk;

	/* Macros for non-VIS memcpy code. */
#ifdef REGS_64BIT

#define MOVE_BIGCHUNK(src, dst, offset, t0, t1, t2, t3)			\
	ASI_SETSRC_NOBLK						\
	LDX			[%src + offset + 0x00] ASINORMAL, %t0; 	\
	LDX			[%src + offset + 0x08] ASINORMAL, %t1; 	\
	LDX			[%src + offset + 0x10] ASINORMAL, %t2; 	\
	LDX			[%src + offset + 0x18] ASINORMAL, %t3; 	\
	ASI_SETDST_NOBLK						\
	STW			%t0, [%dst + offset + 0x04] ASINORMAL; 	\
	srlx			%t0, 32, %t0;				\
	STW			%t0, [%dst + offset + 0x00] ASINORMAL; 	\
	STW			%t1, [%dst + offset + 0x0c] ASINORMAL; 	\
	srlx			%t1, 32, %t1;				\
	STW			%t1, [%dst + offset + 0x08] ASINORMAL; 	\
	STW			%t2, [%dst + offset + 0x14] ASINORMAL; 	\
	srlx			%t2, 32, %t2;				\
	STW			%t2, [%dst + offset + 0x10] ASINORMAL; 	\
	STW			%t3, [%dst + offset + 0x1c] ASINORMAL;	\
	srlx			%t3, 32, %t3;				\
	STW			%t3, [%dst + offset + 0x18] ASINORMAL;

#define MOVE_BIGALIGNCHUNK(src, dst, offset, t0, t1, t2, t3)		\
	ASI_SETSRC_NOBLK						\
	LDX			[%src + offset + 0x00] ASINORMAL, %t0; 	\
	LDX			[%src + offset + 0x08] ASINORMAL, %t1; 	\
	LDX			[%src + offset + 0x10] ASINORMAL, %t2; 	\
	LDX			[%src + offset + 0x18] ASINORMAL, %t3; 	\
	ASI_SETDST_NOBLK						\
	STX			%t0, [%dst + offset + 0x00] ASINORMAL; 	\
	STX			%t1, [%dst + offset + 0x08] ASINORMAL; 	\
	STX			%t2, [%dst + offset + 0x10] ASINORMAL; 	\
	STX			%t3, [%dst + offset + 0x18] ASINORMAL; 	\
	ASI_SETSRC_NOBLK						\
	LDX			[%src + offset + 0x20] ASINORMAL, %t0; 	\
	LDX			[%src + offset + 0x28] ASINORMAL, %t1; 	\
	LDX			[%src + offset + 0x30] ASINORMAL, %t2; 	\
	LDX			[%src + offset + 0x38] ASINORMAL, %t3; 	\
	ASI_SETDST_NOBLK						\
	STX			%t0, [%dst + offset + 0x20] ASINORMAL; 	\
	STX			%t1, [%dst + offset + 0x28] ASINORMAL; 	\
	STX			%t2, [%dst + offset + 0x30] ASINORMAL; 	\
	STX			%t3, [%dst + offset + 0x38] ASINORMAL;

#define MOVE_LASTCHUNK(src, dst, offset, t0, t1, t2, t3)		\
	ASI_SETSRC_NOBLK						\
	LDX			[%src - offset - 0x10] ASINORMAL, %t0;	\
	LDX			[%src - offset - 0x08] ASINORMAL, %t1; 	\
	ASI_SETDST_NOBLK						\
	STW			%t0, [%dst - offset - 0x0c] ASINORMAL; 	\
	srlx			%t0, 32, %t2;				\
	STW			%t2, [%dst - offset - 0x10] ASINORMAL; 	\
	STW			%t1, [%dst - offset - 0x04] ASINORMAL; 	\
	srlx			%t1, 32, %t3;				\
	STW			%t3, [%dst - offset - 0x08] ASINORMAL;

#define MOVE_LASTALIGNCHUNK(src, dst, offset, t0, t1)			\
	ASI_SETSRC_NOBLK						\
	LDX			[%src - offset - 0x10] ASINORMAL, %t0; 	\
	LDX			[%src - offset - 0x08] ASINORMAL, %t1; 	\
	ASI_SETDST_NOBLK						\
	STX			%t0, [%dst - offset - 0x10] ASINORMAL; 	\
	STX			%t1, [%dst - offset - 0x08] ASINORMAL;

#else /* !REGS_64BIT */

#define MOVE_BIGCHUNK(src, dst, offset, t0, t1, t2, t3)			\
	lduw			[%src + offset + 0x00], %t0; 		\
	lduw			[%src + offset + 0x04], %t1; 		\
	lduw			[%src + offset + 0x08], %t2; 		\
	lduw			[%src + offset + 0x0c], %t3; 		\
	stw			%t0, [%dst + offset + 0x00]; 		\
	stw			%t1, [%dst + offset + 0x04]; 		\
	stw			%t2, [%dst + offset + 0x08]; 		\
	stw			%t3, [%dst + offset + 0x0c]; 		\
	lduw			[%src + offset + 0x10], %t0; 		\
	lduw			[%src + offset + 0x14], %t1; 		\
	lduw			[%src + offset + 0x18], %t2; 		\
	lduw			[%src + offset + 0x1c], %t3; 		\
	stw			%t0, [%dst + offset + 0x10]; 		\
	stw			%t1, [%dst + offset + 0x14]; 		\
	stw			%t2, [%dst + offset + 0x18]; 		\
	stw			%t3, [%dst + offset + 0x1c];

#define MOVE_LASTCHUNK(src, dst, offset, t0, t1, t2, t3)		\
	lduw			[%src - offset - 0x10], %t0; 		\
	lduw			[%src - offset - 0x0c], %t1; 		\
	lduw			[%src - offset - 0x08], %t2; 		\
	lduw			[%src - offset - 0x04], %t3; 		\
	stw			%t0, [%dst - offset - 0x10]; 		\
	stw			%t1, [%dst - offset - 0x0c]; 		\
	stw			%t2, [%dst - offset - 0x08]; 		\
	stw			%t3, [%dst - offset - 0x04];

#endif /* !REGS_64BIT */

#ifdef __KERNEL__
		.section	__ex_table,#alloc
		.section	.fixup,#alloc,#execinstr
#endif

		.text
		.align			32
		.globl			memcpy
		.type			memcpy,@function

		.globl			bcopy
		.type			bcopy,@function

#ifdef __KERNEL__
		.globl			__memcpy_begin
__memcpy_begin:

		.globl			__memcpy
		.type			__memcpy,@function

memcpy_private:
__memcpy:
memcpy:		mov		ASI_P, asi_src			! IEU0	Group
		brnz,pt		%o2, __memcpy_entry		! CTI
		 mov		ASI_P, asi_dest			! IEU1
		retl
		 clr		%o0

		.align			32
		.globl			__copy_from_user
		.type			__copy_from_user,@function
__copy_from_user:rd		%asi, asi_src			! IEU0	Group
		brnz,pt		%o2, __memcpy_entry		! CTI
		 mov		ASI_P, asi_dest			! IEU1

		.globl			__copy_to_user
		.type			__copy_to_user,@function
__copy_to_user:	mov		ASI_P, asi_src			! IEU0	Group
		brnz,pt		%o2, __memcpy_entry		! CTI
		 rd		%asi, asi_dest			! IEU1
		retl						! CTI	Group
		 clr		%o0				! IEU0	Group

		.globl			__copy_in_user
		.type			__copy_in_user,@function
__copy_in_user:	rd		%asi, asi_src			! IEU0	Group
		brnz,pt		%o2, __memcpy_entry		! CTI
		 mov		asi_src, asi_dest		! IEU1
		retl						! CTI	Group
		 clr		%o0				! IEU0	Group
#endif

bcopy:		or		%o0, 0, %g3			! IEU0	Group
		addcc		%o1, 0, %o0			! IEU1
		brgez,pt	%o2, memcpy_private		! CTI
		 or		%g3, 0, %o1			! IEU0	Group
		retl						! CTI	Group brk forced
		 clr		%o0				! IEU0


#ifdef __KERNEL__
#define BRANCH_ALWAYS	0x10680000
#define NOP		0x01000000
#define ULTRA3_DO_PATCH(OLD, NEW)	\
	sethi	%hi(NEW), %g1; \
	or	%g1, %lo(NEW), %g1; \
	sethi	%hi(OLD), %g2; \
	or	%g2, %lo(OLD), %g2; \
	sub	%g1, %g2, %g1; \
	sethi	%hi(BRANCH_ALWAYS), %g3; \
	srl	%g1, 2, %g1; \
	or	%g3, %lo(BRANCH_ALWAYS), %g3; \
	or	%g3, %g1, %g3; \
	stw	%g3, [%g2]; \
	sethi	%hi(NOP), %g3; \
	or	%g3, %lo(NOP), %g3; \
	stw	%g3, [%g2 + 0x4]; \
	flush	%g2;
#define ULTRA3_PCACHE_DO_NOP(symbol)	\
	sethi	%hi(symbol##_nop_1_6), %g1; \
	or	%g1, %lo(symbol##_nop_1_6), %g1; \
	sethi	%hi(NOP), %g2; \
	stw	%g2, [%g1 + 0x00]; \
	stw	%g2, [%g1 + 0x04]; \
	flush	%g1 + 0x00; \
	stw	%g2, [%g1 + 0x08]; \
	stw	%g2, [%g1 + 0x0c]; \
	flush	%g1 + 0x08; \
	stw	%g2, [%g1 + 0x10]; \
	stw	%g2, [%g1 + 0x04]; \
	flush	%g1 + 0x10; \
	sethi	%hi(symbol##_nop_2_3), %g1; \
	or	%g1, %lo(symbol##_nop_2_3), %g1; \
	stw	%g2, [%g1 + 0x00]; \
	stw	%g2, [%g1 + 0x04]; \
	flush	%g1 + 0x00; \
	stw	%g2, [%g1 + 0x08]; \
	flush	%g1 + 0x08;

#include <asm/dcu.h>

	.globl	cheetah_patch_copyops
cheetah_patch_copyops:
	ULTRA3_DO_PATCH(memcpy, U3memcpy)
	ULTRA3_DO_PATCH(__copy_from_user, U3copy_from_user)
	ULTRA3_DO_PATCH(__copy_to_user, U3copy_to_user)
	ULTRA3_DO_PATCH(__copy_in_user, U3copy_in_user)
#if 0 /* Causes data corruption, nop out the optimization
       * for now -DaveM
       */
	ldxa			[%g0] ASI_DCU_CONTROL_REG, %g3
	sethi			%uhi(DCU_PE), %o3
	sllx			%o3, 32, %o3
	andcc			%g3, %o3, %g0
	be,pn			%xcc, pcache_disabled
	 nop
#endif
	ULTRA3_PCACHE_DO_NOP(U3memcpy)
	ULTRA3_PCACHE_DO_NOP(U3copy_from_user)
	ULTRA3_PCACHE_DO_NOP(U3copy_to_user)
	ULTRA3_PCACHE_DO_NOP(cheetah_copy_user_page)
#if 0
pcache_disabled:
#endif
	retl
	 nop
#undef BRANCH_ALWAYS
#undef NOP
#undef ULTRA3_DO_PATCH
#endif /* __KERNEL__ */

	.align			32
#ifdef __KERNEL__
	andcc			%o0, 7, %g2			! IEU1	Group
#endif
VIS_enter:
	be,pt			%xcc, dest_is_8byte_aligned	! CTI
#ifdef __KERNEL__
	 nop							! IEU0	Group
#else
	 andcc			%o0, 0x38, %g5			! IEU1	Group
#endif
do_dest_8byte_align:
	mov			8, %g1				! IEU0
	sub			%g1, %g2, %g2			! IEU0	Group
	andcc			%o0, 1, %g0			! IEU1
	be,pt			%icc, 2f			! CTI
	 sub			%o2, %g2, %o2			! IEU0	Group
1:	ASI_SETSRC_NOBLK					! LSU	Group
	EX(LDUB			[%o1] ASINORMAL, %o5, 
				add %o2, %g2)			! Load	Group
	add			%o1, 1, %o1			! IEU0
	add			%o0, 1, %o0			! IEU1
	ASI_SETDST_NOBLK					! LSU	Group
	subcc			%g2, 1, %g2			! IEU1	Group
	be,pn			%xcc, 3f			! CTI
	 EX2(STB		%o5, [%o0 - 1] ASINORMAL,
				add %g2, 1, %g2,
				add %o2, %g2)			! Store
2:	ASI_SETSRC_NOBLK					! LSU	Group
	EX(LDUB			[%o1] ASINORMAL, %o5, 
				add %o2, %g2)			! Load	Group
	add			%o0, 2, %o0			! IEU0
	EX2(LDUB		[%o1 + 1] ASINORMAL, %g3,
				sub %o0, 2, %o0,
				add %o2, %g2)			! Load	Group
	ASI_SETDST_NOBLK					! LSU	Group
	subcc			%g2, 2, %g2			! IEU1	Group
	EX2(STB			%o5, [%o0 - 2] ASINORMAL,
				add %g2, 2, %g2,
				add %o2, %g2)			! Store
	add			%o1, 2, %o1			! IEU0
	bne,pt			%xcc, 2b			! CTI	Group
	 EX2(STB		%g3, [%o0 - 1] ASINORMAL,
				add %g2, 1, %g2,
				add %o2, %g2)			! Store
#ifdef __KERNEL__
3:
dest_is_8byte_aligned:
	VISEntry
	andcc			%o0, 0x38, %g5			! IEU1	Group
#else
3:	andcc			%o0, 0x38, %g5			! IEU1	Group
dest_is_8byte_aligned:
#endif
	be,pt			%icc, dest_is_64byte_aligned	! CTI
	 mov			64, %g1				! IEU0
	fmovd			%f0, %f2			! FPU
	sub			%g1, %g5, %g5			! IEU0	Group
	ASI_SETSRC_NOBLK					! LSU	Group
	alignaddr		%o1, %g0, %g1			! GRU	Group
	EXO2(LDDF		[%g1] ASINORMAL, %f4)		! Load	Group
	sub			%o2, %g5, %o2			! IEU0
1:	EX(LDDF			[%g1 + 0x8] ASINORMAL, %f6,
				add %o2, %g5)			! Load	Group
	add			%g1, 0x8, %g1			! IEU0	Group
	subcc			%g5, 8, %g5			! IEU1
	ASI_SETDST_NOBLK					! LSU	Group
	faligndata		%f4, %f6, %f0			! GRU	Group
	EX2(STDF		%f0, [%o0] ASINORMAL,
				add %g5, 8, %g5,
				add %o2, %g5)			! Store
	add			%o1, 8, %o1			! IEU0	Group
	be,pn			%xcc, dest_is_64byte_aligned	! CTI
	 add			%o0, 8, %o0			! IEU1
	ASI_SETSRC_NOBLK					! LSU	Group
	EX(LDDF			[%g1 + 0x8] ASINORMAL, %f4,
				add %o2, %g5)			! Load	Group
	add			%g1, 8, %g1			! IEU0
	subcc			%g5, 8, %g5			! IEU1
	ASI_SETDST_NOBLK					! LSU	Group
	faligndata		%f6, %f4, %f0			! GRU	Group
	EX2(STDF		%f0, [%o0] ASINORMAL,
				add %g5, 8, %g5,
				add %o2, %g5)			! Store
	add			%o1, 8, %o1			! IEU0
	ASI_SETSRC_NOBLK					! LSU	Group
	bne,pt			%xcc, 1b			! CTI	Group
	 add			%o0, 8, %o0			! IEU0
dest_is_64byte_aligned:
	membar		  #LoadStore | #StoreStore | #StoreLoad	! LSU	Group
#ifndef __KERNEL__
	wr			%g0, ASI_BLK_P, %asi		! LSU	Group
#endif
	subcc			%o2, 0x40, %g7			! IEU1	Group
	mov			%o1, %g1			! IEU0
	andncc			%g7, (0x40 - 1), %g7		! IEU1	Group
	srl			%g1, 3, %g2			! IEU0
	sub			%o2, %g7, %g3			! IEU0	Group
	andn			%o1, (0x40 - 1), %o1		! IEU1
	and			%g2, 7, %g2			! IEU0	Group
	andncc			%g3, 0x7, %g3			! IEU1
	fmovd			%f0, %f2			! FPU
	sub			%g3, 0x10, %g3			! IEU0	Group
	sub			%o2, %g7, %o2			! IEU1
#ifdef __KERNEL__
	or			asi_src, ASI_BLK_OR, asi_src	! IEU0	Group
	or			asi_dest, ASI_BLK_OR, asi_dest	! IEU1
#endif
	alignaddr		%g1, %g0, %g0			! GRU	Group
	add			%g1, %g7, %g1			! IEU0	Group
	subcc			%o2, %g3, %o2			! IEU1
	ASI_SETSRC_BLK						! LSU	Group
	EXVIS1(LDBLK		[%o1 + 0x00] ASIBLK, %f0)	! LSU	Group
	add			%g1, %g3, %g1			! IEU0
	EXVIS1(LDBLK		[%o1 + 0x40] ASIBLK, %f16)	! LSU	Group
	sub			%g7, 0x80, %g7			! IEU0
	EXVIS(LDBLK		[%o1 + 0x80] ASIBLK, %f32)	! LSU	Group
#ifdef __KERNEL__
vispc:	sll			%g2, 9, %g2			! IEU0	Group
	sethi			%hi(vis00), %g5			! IEU1
	or			%g5, %lo(vis00), %g5		! IEU0	Group
	jmpl			%g5 + %g2, %g0			! CTI	Group brk forced
	 addcc			%o1, 0xc0, %o1			! IEU1	Group
#else
								! Clk1	Group 8-(
								! Clk2	Group 8-(
								! Clk3	Group 8-(
								! Clk4	Group 8-(
vispc:	rd			%pc, %g5			! PDU	Group 8-(
	addcc			%g5, %lo(vis00 - vispc), %g5	! IEU1	Group
	sll			%g2, 9, %g2			! IEU0
	jmpl			%g5 + %g2, %g0			! CTI	Group brk forced
	 addcc			%o1, 0xc0, %o1			! IEU1	Group
#endif
	.align			512		/* OK, here comes the fun part... */
vis00:FREG_FROB(f0, f2, f4, f6, f8, f10,f12,f14,f16) LOOP_CHUNK1(o1, o0, g7, vis01)
      FREG_FROB(f16,f18,f20,f22,f24,f26,f28,f30,f32) LOOP_CHUNK2(o1, o0, g7, vis02)
      FREG_FROB(f32,f34,f36,f38,f40,f42,f44,f46,f0)  LOOP_CHUNK3(o1, o0, g7, vis03)
      b,pt			%xcc, vis00+4; faligndata %f0, %f2, %f48
vis01:FREG_FROB(f16,f18,f20,f22,f24,f26,f28,f30,f32) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f32,f34,f36,f38,f40,f42,f44,f46,f0)  STORE_JUMP(o0, f48, finish_f0) membar #Sync
vis02:FREG_FROB(f32,f34,f36,f38,f40,f42,f44,f46,f0)  STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f0, f2, f4, f6, f8, f10,f12,f14,f16) STORE_JUMP(o0, f48, finish_f16) membar #Sync
vis03:FREG_FROB(f0, f2, f4, f6, f8, f10,f12,f14,f16) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f16,f18,f20,f22,f24,f26,f28,f30,f32) STORE_JUMP(o0, f48, finish_f32) membar #Sync
      VISLOOP_PAD
vis10:FREG_FROB(f2, f4, f6, f8, f10,f12,f14,f16,f18) LOOP_CHUNK1(o1, o0, g7, vis11)
      FREG_FROB(f18,f20,f22,f24,f26,f28,f30,f32,f34) LOOP_CHUNK2(o1, o0, g7, vis12)
      FREG_FROB(f34,f36,f38,f40,f42,f44,f46,f0, f2)  LOOP_CHUNK3(o1, o0, g7, vis13)
      b,pt			%xcc, vis10+4; faligndata %f2, %f4, %f48
vis11:FREG_FROB(f18,f20,f22,f24,f26,f28,f30,f32,f34) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f34,f36,f38,f40,f42,f44,f46,f0, f2)  STORE_JUMP(o0, f48, finish_f2) membar #Sync
vis12:FREG_FROB(f34,f36,f38,f40,f42,f44,f46,f0, f2)  STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f2, f4, f6, f8, f10,f12,f14,f16,f18) STORE_JUMP(o0, f48, finish_f18) membar #Sync
vis13:FREG_FROB(f2, f4, f6, f8, f10,f12,f14,f16,f18) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f18,f20,f22,f24,f26,f28,f30,f32,f34) STORE_JUMP(o0, f48, finish_f34) membar #Sync
      VISLOOP_PAD
vis20:FREG_FROB(f4, f6, f8, f10,f12,f14,f16,f18,f20) LOOP_CHUNK1(o1, o0, g7, vis21)
      FREG_FROB(f20,f22,f24,f26,f28,f30,f32,f34,f36) LOOP_CHUNK2(o1, o0, g7, vis22)
      FREG_FROB(f36,f38,f40,f42,f44,f46,f0, f2, f4)  LOOP_CHUNK3(o1, o0, g7, vis23)
      b,pt			%xcc, vis20+4; faligndata %f4, %f6, %f48
vis21:FREG_FROB(f20,f22,f24,f26,f28,f30,f32,f34,f36) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f36,f38,f40,f42,f44,f46,f0, f2, f4)  STORE_JUMP(o0, f48, finish_f4) membar #Sync
vis22:FREG_FROB(f36,f38,f40,f42,f44,f46,f0, f2, f4)  STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f4, f6, f8, f10,f12,f14,f16,f18,f20) STORE_JUMP(o0, f48, finish_f20) membar #Sync
vis23:FREG_FROB(f4, f6, f8, f10,f12,f14,f16,f18,f20) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f20,f22,f24,f26,f28,f30,f32,f34,f36) STORE_JUMP(o0, f48, finish_f36) membar #Sync
      VISLOOP_PAD
vis30:FREG_FROB(f6, f8, f10,f12,f14,f16,f18,f20,f22) LOOP_CHUNK1(o1, o0, g7, vis31)
      FREG_FROB(f22,f24,f26,f28,f30,f32,f34,f36,f38) LOOP_CHUNK2(o1, o0, g7, vis32)
      FREG_FROB(f38,f40,f42,f44,f46,f0, f2, f4, f6)  LOOP_CHUNK3(o1, o0, g7, vis33)
      b,pt			%xcc, vis30+4; faligndata %f6, %f8, %f48
vis31:FREG_FROB(f22,f24,f26,f28,f30,f32,f34,f36,f38) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f38,f40,f42,f44,f46,f0, f2, f4, f6)  STORE_JUMP(o0, f48, finish_f6) membar #Sync
vis32:FREG_FROB(f38,f40,f42,f44,f46,f0, f2, f4, f6)  STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f6, f8, f10,f12,f14,f16,f18,f20,f22) STORE_JUMP(o0, f48, finish_f22) membar #Sync
vis33:FREG_FROB(f6, f8, f10,f12,f14,f16,f18,f20,f22) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f22,f24,f26,f28,f30,f32,f34,f36,f38) STORE_JUMP(o0, f48, finish_f38) membar #Sync
      VISLOOP_PAD
vis40:FREG_FROB(f8, f10,f12,f14,f16,f18,f20,f22,f24) LOOP_CHUNK1(o1, o0, g7, vis41)
      FREG_FROB(f24,f26,f28,f30,f32,f34,f36,f38,f40) LOOP_CHUNK2(o1, o0, g7, vis42)
      FREG_FROB(f40,f42,f44,f46,f0, f2, f4, f6, f8)  LOOP_CHUNK3(o1, o0, g7, vis43)
      b,pt			%xcc, vis40+4; faligndata %f8, %f10, %f48
vis41:FREG_FROB(f24,f26,f28,f30,f32,f34,f36,f38,f40) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f40,f42,f44,f46,f0, f2, f4, f6, f8)  STORE_JUMP(o0, f48, finish_f8) membar #Sync
vis42:FREG_FROB(f40,f42,f44,f46,f0, f2, f4, f6, f8)  STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f8, f10,f12,f14,f16,f18,f20,f22,f24) STORE_JUMP(o0, f48, finish_f24) membar #Sync
vis43:FREG_FROB(f8, f10,f12,f14,f16,f18,f20,f22,f24) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f24,f26,f28,f30,f32,f34,f36,f38,f40) STORE_JUMP(o0, f48, finish_f40) membar #Sync
      VISLOOP_PAD
vis50:FREG_FROB(f10,f12,f14,f16,f18,f20,f22,f24,f26) LOOP_CHUNK1(o1, o0, g7, vis51)
      FREG_FROB(f26,f28,f30,f32,f34,f36,f38,f40,f42) LOOP_CHUNK2(o1, o0, g7, vis52)
      FREG_FROB(f42,f44,f46,f0, f2, f4, f6, f8, f10) LOOP_CHUNK3(o1, o0, g7, vis53)
      b,pt			%xcc, vis50+4; faligndata %f10, %f12, %f48
vis51:FREG_FROB(f26,f28,f30,f32,f34,f36,f38,f40,f42) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f42,f44,f46,f0, f2, f4, f6, f8, f10) STORE_JUMP(o0, f48, finish_f10) membar #Sync
vis52:FREG_FROB(f42,f44,f46,f0, f2, f4, f6, f8, f10) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f10,f12,f14,f16,f18,f20,f22,f24,f26) STORE_JUMP(o0, f48, finish_f26) membar #Sync
vis53:FREG_FROB(f10,f12,f14,f16,f18,f20,f22,f24,f26) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f26,f28,f30,f32,f34,f36,f38,f40,f42) STORE_JUMP(o0, f48, finish_f42) membar #Sync
      VISLOOP_PAD
vis60:FREG_FROB(f12,f14,f16,f18,f20,f22,f24,f26,f28) LOOP_CHUNK1(o1, o0, g7, vis61)
      FREG_FROB(f28,f30,f32,f34,f36,f38,f40,f42,f44) LOOP_CHUNK2(o1, o0, g7, vis62)
      FREG_FROB(f44,f46,f0, f2, f4, f6, f8, f10,f12) LOOP_CHUNK3(o1, o0, g7, vis63)
      b,pt			%xcc, vis60+4; faligndata %f12, %f14, %f48
vis61:FREG_FROB(f28,f30,f32,f34,f36,f38,f40,f42,f44) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f44,f46,f0, f2, f4, f6, f8, f10,f12) STORE_JUMP(o0, f48, finish_f12) membar #Sync
vis62:FREG_FROB(f44,f46,f0, f2, f4, f6, f8, f10,f12) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f12,f14,f16,f18,f20,f22,f24,f26,f28) STORE_JUMP(o0, f48, finish_f28) membar #Sync
vis63:FREG_FROB(f12,f14,f16,f18,f20,f22,f24,f26,f28) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f28,f30,f32,f34,f36,f38,f40,f42,f44) STORE_JUMP(o0, f48, finish_f44) membar #Sync
      VISLOOP_PAD
vis70:FREG_FROB(f14,f16,f18,f20,f22,f24,f26,f28,f30) LOOP_CHUNK1(o1, o0, g7, vis71)
      FREG_FROB(f30,f32,f34,f36,f38,f40,f42,f44,f46) LOOP_CHUNK2(o1, o0, g7, vis72)
      FREG_FROB(f46,f0, f2, f4, f6, f8, f10,f12,f14) LOOP_CHUNK3(o1, o0, g7, vis73)
      b,pt			%xcc, vis70+4; faligndata %f14, %f16, %f48
vis71:FREG_FROB(f30,f32,f34,f36,f38,f40,f42,f44,f46) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f46,f0, f2, f4, f6, f8, f10,f12,f14) STORE_JUMP(o0, f48, finish_f14) membar #Sync
vis72:FREG_FROB(f46,f0, f2, f4, f6, f8, f10,f12,f14) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f14,f16,f18,f20,f22,f24,f26,f28,f30) STORE_JUMP(o0, f48, finish_f30) membar #Sync
vis73:FREG_FROB(f14,f16,f18,f20,f22,f24,f26,f28,f30) STORE_SYNC(o0, f48) membar #Sync
      FREG_FROB(f30,f32,f34,f36,f38,f40,f42,f44,f46) STORE_JUMP(o0, f48, finish_f46) membar #Sync
      VISLOOP_PAD
finish_f0:	FINISH_VISCHUNK(o0, f0,  f2,  g3)
finish_f2:	FINISH_VISCHUNK(o0, f2,  f4,  g3)
finish_f4:	FINISH_VISCHUNK(o0, f4,  f6,  g3)
finish_f6:	FINISH_VISCHUNK(o0, f6,  f8,  g3)
finish_f8:	FINISH_VISCHUNK(o0, f8,  f10, g3)
finish_f10:	FINISH_VISCHUNK(o0, f10, f12, g3)
finish_f12:	FINISH_VISCHUNK(o0, f12, f14, g3)
finish_f14:	UNEVEN_VISCHUNK(o0, f14, f0,  g3)
finish_f16:	FINISH_VISCHUNK(o0, f16, f18, g3)
finish_f18:	FINISH_VISCHUNK(o0, f18, f20, g3)
finish_f20:	FINISH_VISCHUNK(o0, f20, f22, g3)
finish_f22:	FINISH_VISCHUNK(o0, f22, f24, g3)
finish_f24:	FINISH_VISCHUNK(o0, f24, f26, g3)
finish_f26:	FINISH_VISCHUNK(o0, f26, f28, g3)
finish_f28:	FINISH_VISCHUNK(o0, f28, f30, g3)
finish_f30:	UNEVEN_VISCHUNK(o0, f30, f0,  g3)
finish_f32:	FINISH_VISCHUNK(o0, f32, f34, g3)
finish_f34:	FINISH_VISCHUNK(o0, f34, f36, g3)
finish_f36:	FINISH_VISCHUNK(o0, f36, f38, g3)
finish_f38:	FINISH_VISCHUNK(o0, f38, f40, g3)
finish_f40:	FINISH_VISCHUNK(o0, f40, f42, g3)
finish_f42:	FINISH_VISCHUNK(o0, f42, f44, g3)
finish_f44:	FINISH_VISCHUNK(o0, f44, f46, g3)
finish_f46:	UNEVEN_VISCHUNK_LAST(o0, f46, f0,  g3)
vis_out_slk:
#ifdef __KERNEL__
	srl		asi_src, 3, %g5				! IEU0	Group
	xor		asi_src, ASI_BLK_XOR1, asi_src		! IEU1
	xor		asi_src, %g5, asi_src			! IEU0	Group
#endif
vis_slk:ASI_SETSRC_NOBLK					! LSU	Group
	EXVIS3(LDDF	[%o1] ASINORMAL, %f2)			! Load	Group
	add		%o1, 8, %o1				! IEU0
	subcc		%g3, 8, %g3				! IEU1
	ASI_SETDST_NOBLK					! LSU	Group
	faligndata	%f0, %f2, %f8				! GRU	Group
	EXVIS4(STDF	%f8, [%o0] ASINORMAL)			! Store
	bl,pn		%xcc, vis_out_slp			! CTI
	 add		%o0, 8, %o0				! IEU0	Group
	ASI_SETSRC_NOBLK					! LSU	Group
	EXVIS3(LDDF	[%o1] ASINORMAL, %f0)			! Load	Group
	add		%o1, 8, %o1				! IEU0
	subcc		%g3, 8, %g3				! IEU1
	ASI_SETDST_NOBLK					! LSU	Group
	faligndata	%f2, %f0, %f8				! GRU	Group
	EXVIS4(STDF	%f8, [%o0] ASINORMAL)			! Store
	bge,pt		%xcc, vis_slk				! CTI
	 add		%o0, 8, %o0				! IEU0	Group
vis_out_slp:
#ifdef __KERNEL__
	brz,pt		%o2, vis_ret				! CTI	Group
	 mov		%g1, %o1				! IEU0
	ba,pt		%xcc, vis_slp+4				! CTI	Group
	 ASI_SETSRC_NOBLK					! LSU	Group
#endif
vis_out:brz,pt		%o2, vis_ret				! CTI	Group
	 mov		%g1, %o1				! IEU0
#ifdef __KERNEL__
	srl		asi_src, 3, %g5				! IEU0	Group
	xor		asi_src, ASI_BLK_XOR1, asi_src		! IEU1
	xor		asi_src, %g5, asi_src			! IEU0	Group
#endif
vis_slp:ASI_SETSRC_NOBLK					! LSU	Group
	EXO2(LDUB	[%o1] ASINORMAL, %g5)			! LOAD
	add		%o1, 1, %o1				! IEU0
	add		%o0, 1, %o0				! IEU1
	ASI_SETDST_NOBLK					! LSU	Group
	subcc		%o2, 1, %o2				! IEU1
	bne,pt		%xcc, vis_slp				! CTI
	 EX(STB		%g5, [%o0 - 1] ASINORMAL,
			add %o2, 1)				! Store	Group
vis_ret:membar		#StoreLoad | #StoreStore		! LSU	Group
	FPU_CLEAN_RETL


__memcpy_short:
	andcc		%o2, 1, %g0				! IEU1	Group
	be,pt		%icc, 2f				! CTI
1:	 ASI_SETSRC_NOBLK					! LSU	Group
	EXO2(LDUB	[%o1] ASINORMAL, %g5)			! LOAD	Group
	add		%o1, 1, %o1				! IEU0
	add		%o0, 1, %o0				! IEU1
	ASI_SETDST_NOBLK					! LSU	Group
	subcc		%o2, 1, %o2				! IEU1	Group
	be,pn		%xcc, short_ret				! CTI
	 EX(STB		%g5, [%o0 - 1] ASINORMAL,
			add %o2, 1)				! Store
2:	ASI_SETSRC_NOBLK					! LSU	Group
	EXO2(LDUB	[%o1] ASINORMAL, %g5)			! LOAD	Group
	add		%o0, 2, %o0				! IEU0
	EX2(LDUB	[%o1 + 1] ASINORMAL, %o5,
			sub %o0, 2, %o0,
			add %o2, %g0)				! LOAD	Group
	add		%o1, 2, %o1				! IEU0
	ASI_SETDST_NOBLK					! LSU	Group
	subcc		%o2, 2, %o2				! IEU1	Group
	EX(STB		%g5, [%o0 - 2] ASINORMAL,
			add %o2, 2)				! Store
	bne,pt		%xcc, 2b				! CTI
	 EX(STB		%o5, [%o0 - 1] ASINORMAL,
			add %o2, 1)				! Store
short_ret:
	NORMAL_RETL

#ifndef __KERNEL__
memcpy_private:
memcpy:
#ifndef REGS_64BIT
	srl		%o2, 0, %o2				! IEU1	Group
#endif	
	brz,pn		%o2, short_ret				! CTI	Group
	 mov		%o0, %g6				! IEU0
#endif
__memcpy_entry:
	cmp		%o2, 15					! IEU1	Group
	bleu,pn		%xcc, __memcpy_short			! CTI
	 cmp		%o2, (64 * 6)				! IEU1	Group
	bgeu,pn		%xcc, VIS_enter				! CTI
	 andcc		%o0, 7, %g2				! IEU1	Group
	sub		%o0, %o1, %g5				! IEU0
	andcc		%g5, 3, %o5				! IEU1	Group
	bne,pn		%xcc, memcpy_noVIS_misaligned		! CTI
	 andcc		%o1, 3, %g0				! IEU1	Group
#ifdef REGS_64BIT
	be,a,pt		%xcc, 3f				! CTI
	 andcc		%o1, 4, %g0				! IEU1	Group
	andcc		%o1, 1, %g0				! IEU1	Group
#else /* !REGS_64BIT */
	be,pt		%xcc, 5f				! CTI
	 andcc		%o1, 1, %g0				! IEU1	Group
#endif /* !REGS_64BIT */
	be,pn		%xcc, 4f				! CTI
	 andcc		%o1, 2, %g0				! IEU1	Group
	ASI_SETSRC_NOBLK					! LSU	Group
	EXO2(LDUB	[%o1] ASINORMAL, %g2)			! Load	Group
	add		%o1, 1, %o1				! IEU0
	add		%o0, 1, %o0				! IEU1
	sub		%o2, 1, %o2				! IEU0	Group
	ASI_SETDST_NOBLK					! LSU	Group
	bne,pn		%xcc, 5f				! CTI	Group
	 EX(STB		%g2, [%o0 - 1] ASINORMAL,
			add %o2, 1)				! Store
4:	ASI_SETSRC_NOBLK					! LSU	Group
	EXO2(LDUH	[%o1] ASINORMAL, %g2)			! Load	Group
	add		%o1, 2, %o1				! IEU0
	add		%o0, 2, %o0				! IEU1
	ASI_SETDST_NOBLK					! LSU	Group
	sub		%o2, 2, %o2				! IEU0
	EX(STH		%g2, [%o0 - 2] ASINORMAL,
			add %o2, 2)				! Store	Group + bubble
#ifdef REGS_64BIT
5:	andcc		%o1, 4, %g0				! IEU1
3:	be,a,pn		%xcc, 2f				! CTI
	 andcc		%o2, -128, %g7				! IEU1	Group
	ASI_SETSRC_NOBLK					! LSU	Group
	EXO2(LDUW	[%o1] ASINORMAL, %g5)			! Load	Group
	add		%o1, 4, %o1				! IEU0
	add		%o0, 4, %o0				! IEU1
	ASI_SETDST_NOBLK					! LSU	Group
	sub		%o2, 4, %o2				! IEU0	Group
	EX(STW		%g5, [%o0 - 4] ASINORMAL,
			add %o2, 4)				! Store
	andcc		%o2, -128, %g7				! IEU1	Group
2:	be,pn		%xcc, 3f				! CTI
	 andcc		%o0, 4, %g0				! IEU1	Group
	be,pn		%xcc, 82f + 4				! CTI	Group
#else /* !REGS_64BIT */
5:	andcc		%o2, -128, %g7				! IEU1
	be,a,pn		%xcc, 41f				! CTI
	 andcc		%o2, 0x70, %g7				! IEU1	Group
#endif /* !REGS_64BIT */
5:	MOVE_BIGCHUNK(o1, o0, 0x00, g1, g3, g5, o5)
	MOVE_BIGCHUNK(o1, o0, 0x20, g1, g3, g5, o5)
	MOVE_BIGCHUNK(o1, o0, 0x40, g1, g3, g5, o5)
	MOVE_BIGCHUNK(o1, o0, 0x60, g1, g3, g5, o5)
	EXT(5b,35f,VIScopyfixup1)
35:	subcc		%g7, 128, %g7				! IEU1	Group
	add		%o1, 128, %o1				! IEU0
	bne,pt		%xcc, 5b				! CTI
	 add		%o0, 128, %o0				! IEU0	Group
3:	andcc		%o2, 0x70, %g7				! IEU1	Group
41:	be,pn		%xcc, 80f				! CTI
	 andcc		%o2, 8, %g0				! IEU1	Group
#ifdef __KERNEL__
79:	sethi		%hi(80f), %o5				! IEU0
	sll		%g7, 1, %g5				! IEU0	Group
	add		%o1, %g7, %o1				! IEU1
	srl		%g7, 1, %g2				! IEU0  Group
	sub		%o5, %g5, %o5				! IEU1
	sub		%o5, %g2, %o5				! IEU0  Group
	jmpl		%o5 + %lo(80f), %g0			! CTI	Group brk forced
	 add		%o0, %g7, %o0				! IEU0	Group
#else
								! Clk1 8-(
								! Clk2 8-(
								! Clk3 8-(
								! Clk4 8-(
79:	rd		%pc, %o5				! PDU	Group
	sll		%g7, 1, %g5				! IEU0	Group
	add		%o1, %g7, %o1				! IEU1
	sub		%o5, %g5, %o5				! IEU0  Group
	jmpl		%o5 + %lo(80f - 79b), %g0		! CTI	Group brk forced
	 add		%o0, %g7, %o0				! IEU0	Group
#endif
36:	MOVE_LASTCHUNK(o1, o0, 0x60, g2, g3, g5, o5)
	MOVE_LASTCHUNK(o1, o0, 0x50, g2, g3, g5, o5)
	MOVE_LASTCHUNK(o1, o0, 0x40, g2, g3, g5, o5)
	MOVE_LASTCHUNK(o1, o0, 0x30, g2, g3, g5, o5)
	MOVE_LASTCHUNK(o1, o0, 0x20, g2, g3, g5, o5)
	MOVE_LASTCHUNK(o1, o0, 0x10, g2, g3, g5, o5)
	MOVE_LASTCHUNK(o1, o0, 0x00, g2, g3, g5, o5)
	EXT(36b,80f,VIScopyfixup2)
80:	be,pt		%xcc, 81f				! CTI
	 andcc		%o2, 4, %g0				! IEU1
#ifdef REGS_64BIT
	ASI_SETSRC_NOBLK					! LSU	Group
	EX(LDX		[%o1] ASINORMAL, %g2,
			and %o2, 0xf)				! Load	Group
	add		%o0, 8, %o0				! IEU0
	ASI_SETDST_NOBLK					! LSU	Group
	EX(STW		%g2, [%o0 - 0x4] ASINORMAL,
			and %o2, 0xf)				! Store	Group
	add		%o1, 8, %o1				! IEU1
	srlx		%g2, 32, %g2				! IEU0	Group
	EX2(STW		%g2, [%o0 - 0x8] ASINORMAL,
			and %o2, 0xf, %o2,
			sub %o2, 4)				! Store
#else /* !REGS_64BIT */
	lduw		[%o1], %g2				! Load	Group
	add		%o0, 8, %o0				! IEU0
	lduw		[%o1 + 0x4], %g3			! Load	Group
	add		%o1, 8, %o1				! IEU0
	stw		%g2, [%o0 - 0x8]			! Store	Group
	stw		%g3, [%o0 - 0x4]			! Store	Group
#endif /* !REGS_64BIT */
81:	be,pt		%xcc, 1f				! CTI
	 andcc		%o2, 2, %g0				! IEU1	Group
	ASI_SETSRC_NOBLK					! LSU	Group
	EX(LDUW		[%o1] ASINORMAL, %g2,
			and %o2, 0x7)				! Load	Group
	add		%o1, 4, %o1				! IEU0
	ASI_SETDST_NOBLK					! LSU	Group
	EX(STW		%g2, [%o0] ASINORMAL,
			and %o2, 0x7)				! Store	Group
	add		%o0, 4, %o0				! IEU0
1:	be,pt		%xcc, 1f				! CTI
	 andcc		%o2, 1, %g0				! IEU1	Group
	ASI_SETSRC_NOBLK					! LSU	Group
	EX(LDUH		[%o1] ASINORMAL, %g2,
			and %o2, 0x3)				! Load	Group
	add		%o1, 2, %o1				! IEU0
	ASI_SETDST_NOBLK					! LSU	Group
	EX(STH		%g2, [%o0] ASINORMAL,
			and %o2, 0x3)				! Store	Group
	add		%o0, 2, %o0				! IEU0
1:	be,pt		%xcc, normal_retl			! CTI
	 nop							! IEU1
	ASI_SETSRC_NOBLK					! LSU	Group
	EX(LDUB		[%o1] ASINORMAL, %g2,
			add %g0, 1)				! Load	Group
	ASI_SETDST_NOBLK					! LSU	Group
	EX(STB		%g2, [%o0] ASINORMAL,
			add %g0, 1)				! Store	Group + bubble
normal_retl:
	NORMAL_RETL

#ifdef REGS_64BIT
82:	MOVE_BIGALIGNCHUNK(o1, o0, 0x00, g1, g3, g5, o5)
	MOVE_BIGALIGNCHUNK(o1, o0, 0x40, g1, g3, g5, o5)
	EXT(82b,37f,VIScopyfixup3)
37:	subcc		%g7, 128, %g7				! IEU1	Group
	add		%o1, 128, %o1				! IEU0
	bne,pt		%xcc, 82b				! CTI
	 add		%o0, 128, %o0				! IEU0	Group
	andcc		%o2, 0x70, %g7				! IEU1
	be,pn		%xcc, 84f				! CTI
	 andcc		%o2, 8, %g0				! IEU1	Group
#ifdef __KERNEL__
83:	srl		%g7, 1, %g5				! IEU0
	sethi		%hi(84f), %o5				! IEU0	Group
	add		%g7, %g5, %g5				! IEU1
	add		%o1, %g7, %o1				! IEU0	Group
	sub		%o5, %g5, %o5				! IEU1
	jmpl		%o5 + %lo(84f), %g0			! CTI	Group brk forced
	 add		%o0, %g7, %o0				! IEU0	Group
#else
								! Clk1 8-(
								! Clk2 8-(
								! Clk3 8-(
								! Clk4 8-(
83:	rd		%pc, %o5				! PDU	Group
	add		%o1, %g7, %o1				! IEU0	Group
	sub		%o5, %g7, %o5				! IEU1
	jmpl		%o5 + %lo(84f - 83b), %g0		! CTI	Group brk forced
	 add		%o0, %g7, %o0				! IEU0	Group
#endif
38:	MOVE_LASTALIGNCHUNK(o1, o0, 0x60, g2, g3)
	MOVE_LASTALIGNCHUNK(o1, o0, 0x50, g2, g3)
	MOVE_LASTALIGNCHUNK(o1, o0, 0x40, g2, g3)
	MOVE_LASTALIGNCHUNK(o1, o0, 0x30, g2, g3)
	MOVE_LASTALIGNCHUNK(o1, o0, 0x20, g2, g3)
	MOVE_LASTALIGNCHUNK(o1, o0, 0x10, g2, g3)
	MOVE_LASTALIGNCHUNK(o1, o0, 0x00, g2, g3)
	EXT(38b,84f,VIScopyfixup4)
84:	be,pt		%xcc, 85f				! CTI	Group
	 andcc		%o2, 4, %g0				! IEU1
	ASI_SETSRC_NOBLK					! LSU	Group
	EX(LDX		[%o1] ASINORMAL, %g2,
			and %o2, 0xf)				! Load	Group
	add		%o0, 8, %o0				! IEU0
	ASI_SETDST_NOBLK					! LSU	Group
	add		%o1, 8, %o1				! IEU0	Group
	EX(STX		%g2, [%o0 - 0x8] ASINORMAL,
			and %o2, 0xf)				! Store
85:	be,pt		%xcc, 1f				! CTI
	 andcc		%o2, 2, %g0				! IEU1	Group
	ASI_SETSRC_NOBLK					! LSU	Group
	EX(LDUW		[%o1] ASINORMAL, %g2,
			and %o2, 0x7)				! Load	Group
	add		%o0, 4, %o0				! IEU0
	ASI_SETDST_NOBLK					! LSU	Group
	add		%o1, 4, %o1				! IEU0	Group
	EX(STW		%g2, [%o0 - 0x4] ASINORMAL,
			and %o2, 0x7)				! Store
1:	be,pt		%xcc, 1f				! CTI
	 andcc		%o2, 1, %g0				! IEU1	Group
	ASI_SETSRC_NOBLK					! LSU	Group
	EX(LDUH		[%o1] ASINORMAL, %g2,
			and %o2, 0x3)				! Load	Group
	add		%o0, 2, %o0				! IEU0
	ASI_SETDST_NOBLK					! LSU	Group
	add		%o1, 2, %o1				! IEU0	Group
	EX(STH		%g2, [%o0 - 0x2] ASINORMAL,
			and %o2, 0x3)				! Store
1:	be,pt		%xcc, 1f				! CTI
	 nop							! IEU0	Group
	ASI_SETSRC_NOBLK					! LSU	Group
	EX(LDUB		[%o1] ASINORMAL, %g2,
			add %g0, 1)				! Load	Group
	ASI_SETDST_NOBLK					! LSU	Group
	EX(STB		%g2, [%o0] ASINORMAL,
			add %g0, 1)				! Store	Group + bubble
1:	NORMAL_RETL
#endif	/* REGS_64BIT */

memcpy_noVIS_misaligned:
	brz,pt			%g2, 2f				! CTI	Group
	 mov			8, %g1				! IEU0
	sub			%g1, %g2, %g2			! IEU0	Group
	sub			%o2, %g2, %o2			! IEU0	Group
1:	ASI_SETSRC_NOBLK					! LSU	Group
	EX(LDUB			[%o1] ASINORMAL, %g5,
				add %o2, %g2)			! Load	Group
	add			%o1, 1, %o1			! IEU0
	add			%o0, 1, %o0			! IEU1
	ASI_SETDST_NOBLK					! LSU	Group
	subcc			%g2, 1, %g2			! IEU1	Group
	bne,pt			%xcc, 1b			! CTI
	 EX2(STB		%g5, [%o0 - 1] ASINORMAL,
				add %o2, %g2, %o2,
				add %o2, 1)			! Store
2:
#ifdef __KERNEL__
	VISEntry
#endif
	andn			%o2, 7, %g5 			! IEU0	Group
	and			%o2, 7, %o2			! IEU1
	fmovd			%f0, %f2			! FPU
	ASI_SETSRC_NOBLK					! LSU	Group
	alignaddr		%o1, %g0, %g1			! GRU	Group
	EXO2(LDDF		[%g1] ASINORMAL, %f4)		! Load	Group
1:	EX(LDDF			[%g1 + 0x8] ASINORMAL, %f6,
				add %o2, %g5)			! Load	Group
	add			%g1, 0x8, %g1			! IEU0	Group
	subcc			%g5, 8, %g5			! IEU1
	ASI_SETDST_NOBLK					! LSU	Group
	faligndata		%f4, %f6, %f0			! GRU	Group
	EX2(STDF		%f0, [%o0] ASINORMAL,
				add %o2, %g5, %o2,
				add %o2, 8)			! Store
	add			%o1, 8, %o1			! IEU0	Group
	be,pn			%xcc, end_cruft			! CTI
	 add			%o0, 8, %o0			! IEU1
	ASI_SETSRC_NOBLK					! LSU	Group
	EX(LDDF			[%g1 + 0x8] ASINORMAL, %f4,
				add %o2, %g5)			! Load	Group
	add			%g1, 8, %g1			! IEU0
	subcc			%g5, 8, %g5			! IEU1
	ASI_SETDST_NOBLK					! LSU	Group
	faligndata		%f6, %f4, %f0			! GRU	Group
	EX2(STDF		%f0, [%o0] ASINORMAL,
				add %o2, %g5, %o2,
				add %o2, 8)			! Store
	add			%o1, 8, %o1			! IEU0
	ASI_SETSRC_NOBLK					! LSU	Group
	bne,pn			%xcc, 1b			! CTI	Group
	 add			%o0, 8, %o0			! IEU0
end_cruft:
	brz,pn			%o2, fpu_retl			! CTI	Group
#ifndef __KERNEL__
	 nop							! IEU0
#else
	 ASI_SETSRC_NOBLK					! LSU	Group
#endif
	EXO2(LDUB	[%o1] ASINORMAL, %g5)			! LOAD
	add		%o1, 1, %o1				! IEU0
	add		%o0, 1, %o0				! IEU1
	ASI_SETDST_NOBLK					! LSU	Group
	subcc		%o2, 1, %o2				! IEU1
	bne,pt		%xcc, vis_slp				! CTI
	 EX(STB		%g5, [%o0 - 1] ASINORMAL,
			add %o2, 1)				! Store	Group
fpu_retl:
	FPU_RETL

#ifdef __KERNEL__
	.globl		__memcpy_end
__memcpy_end:

		.section	.fixup
		.align		4
VIScopyfixup_reto2:
		mov		%o2, %o1
VIScopyfixup_ret:
		/* If this is copy_from_user(), zero out the rest of the
		 * kernel buffer.
		 */
		ldub		[%g6 + AOFF_task_thread + AOFF_thread_current_ds], %o4
		andcc		asi_src, 0x1, %g0
		be,pt		%icc, 1f
		 VISExit
		andcc		asi_dest, 0x1, %g0
		bne,pn		%icc, 1f
		 nop
		save		%sp, -160, %sp
		mov		%i0, %o0
		call		__bzero
		 mov		%i1, %o1
		restore
1:		mov		%o1, %o0
		retl
		 wr		%o4, %g0, %asi
VIScopyfixup1:	subcc		%g2, 18, %g2
		add		%o0, 32, %o0
		bgeu,a,pt	%icc, VIScopyfixup1
		 sub		%g7, 32, %g7
		sub		%o0, 32, %o0
		rd		%pc, %g5
		add		%g2, (18 + 16), %g2
		ldub		[%g5 + %g2], %g2
		ba,a,pt		%xcc, 2f
.byte		0, 0, 0, 0, 0, 0, 0, 4, 4, 8, 12, 12, 16, 20, 20, 24, 28, 28
		.align		4
VIScopyfixup2:	mov		(7 * 16), %g7
1:		subcc		%g2, 10, %g2
		bgeu,a,pt	%icc, 1b
		 sub		%g7, 16, %g7
		sub		%o0, %g7, %o0
		rd		%pc, %g5
		add		%g2, (10 + 16), %g2
		ldub		[%g5 + %g2], %g2
		ba,a,pt		%xcc, 4f
.byte		0, 0, 0, 0, 0, 4, 4, 8, 12, 12
		.align		4
VIScopyfixup3:	subcc		%g2, 10, %g2
		add		%o0, 32, %o0
		bgeu,a,pt	%icc, VIScopyfixup3
		 sub		%g7, 32, %g7
		sub		%o0, 32, %o0
		rd		%pc, %g5
		add		%g2, (10 + 16), %g2
		ldub		[%g5 + %g2], %g2
		ba,a,pt		%xcc, 2f
.byte		0, 0, 0, 0, 0, 0, 0, 8, 16, 24
		.align		4
2:		and		%o2, 0x7f, %o2
		sub		%g7, %g2, %g7
		ba,pt		%xcc, VIScopyfixup_ret
		 add		%g7, %o2, %o1
VIScopyfixup4:	mov		(7 * 16), %g7
3:		subcc		%g2, 6, %g2
		bgeu,a,pt	%icc, 3b
		 sub		%g7, 16, %g7
		sub		%o0, %g7, %o0
		rd		%pc, %g5
		add		%g2, (6 + 16), %g2
		ldub		[%g5 + %g2], %g2
		ba,a,pt		%xcc, 4f
.byte		0, 0, 0, 0, 0, 8
		.align		4
4:		and		%o2, 0xf, %o2
		sub		%g7, %g2, %g7
		ba,pt		%xcc, VIScopyfixup_ret
		 add		%g7, %o2, %o1
VIScopyfixup_vis2:
		sub		%o2, 0x40, %o2
VIScopyfixup_vis0:
		add		%o2, 0x80, %o2
VIScopyfixup_vis1:
		add		%g7, %g3, %g7
		ba,pt		%xcc, VIScopyfixup_ret
		 add		%o2, %g7, %o1
VIScopyfixup_vis4:
		add		%g3, 8, %g3
VIScopyfixup_vis3:
		add		%g3, 8, %g3
		ba,pt		%xcc, VIScopyfixup_ret
		 add		%o2, %g3, %o1
#endif

#ifdef __KERNEL__
		.text
		.align		32

		.globl		__memmove
		.type		__memmove,@function

		.globl		memmove
		.type		memmove,@function

memmove:
__memmove:	cmp		%o0, %o1
		blu,pt		%xcc, memcpy_private
		 sub		%o0, %o1, %g5
		add		%o1, %o2, %g3
		cmp		%g3, %o0
		bleu,pt		%xcc, memcpy_private
		 add		%o1, %o2, %g5
		add		%o0, %o2, %o5

		sub		%g5, 1, %o1
		sub		%o5, 1, %o0
1:		ldub		[%o1], %g5
		subcc		%o2, 1, %o2
		sub		%o1, 1, %o1
		stb		%g5, [%o0]
		bne,pt		%icc, 1b
		 sub		%o0, 1, %o0

		retl
		 clr		%o0
#endif
