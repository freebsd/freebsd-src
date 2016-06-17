/* $Id: blockops.S,v 1.41 2001/12/05 06:05:35 davem Exp $
 * blockops.S: UltraSparc block zero optimized routines.
 *
 * Copyright (C) 1996, 1998, 1999, 2000 David S. Miller (davem@redhat.com)
 * Copyright (C) 1997 Jakub Jelinek (jakub@redhat.com)
 */

#include "VIS.h"
#include <asm/visasm.h>
#include <asm/page.h>
#include <asm/dcu.h>
#include <asm/spitfire.h>
#include <asm/pgtable.h>
#include <asm/asm_offsets.h>

#define TOUCH(reg0, reg1, reg2, reg3, reg4, reg5, reg6, reg7)	\
	fmovd	%reg0, %f48; 	fmovd	%reg1, %f50;		\
	fmovd	%reg2, %f52; 	fmovd	%reg3, %f54;		\
	fmovd	%reg4, %f56; 	fmovd	%reg5, %f58;		\
	fmovd	%reg6, %f60; 	fmovd	%reg7, %f62;

#define	DCACHE_SIZE	(PAGE_SIZE * 2)
#define TLBTEMP_ENT1	(60 << 3)
#define TLBTEMP_ENT2	(61 << 3)
#define TLBTEMP_ENTSZ	(1 << 3)

#if (PAGE_SHIFT == 13) || (PAGE_SHIFT == 19)
#define PAGE_SIZE_REM	0x80
#elif (PAGE_SHIFT == 16) || (PAGE_SHIFT == 22)
#define PAGE_SIZE_REM	0x100
#else
#error Wrong PAGE_SHIFT specified
#endif

	.text

	.align		32
	.globl		copy_user_page
	.type		copy_user_page,@function
copy_user_page: /* %o0=dest, %o1=src, %o2=vaddr */
	VISEntry
	sethi		%hi(PAGE_SIZE), %g3
	sub		%o0, %g4, %g1
	and		%o2, %g3, %o0
	sethi		%hi(TLBTEMP_BASE), %o3
	sethi		%uhi(_PAGE_VALID | _PAGE_SZBITS), %g3
	sub		%o1, %g4, %g2
	sllx		%g3, 32, %g3
	mov		TLB_TAG_ACCESS, %o2
	or		%g3, (_PAGE_CP | _PAGE_CV | _PAGE_P | _PAGE_L | _PAGE_W), %g3
	sethi		%hi(DCACHE_SIZE), %o1
	or		%g1, %g3, %g1
	or		%g2, %g3, %g2
	add		%o0, %o3, %o0
	add		%o0, %o1, %o1
#define FIX_INSN_1	0x96102060 /* mov (12 << 3), %o3 */
cheetah_patch_1:
	mov		TLBTEMP_ENT1, %o3
	rdpr		%pstate, %g3
	wrpr		%g3, PSTATE_IE, %pstate

	/* Do this now, before loading the fixed TLB entries for copying,
	 * so we do not risk a multiple TLB match condition later when
	 * restoring those entries.
	 */
	ldub		[%g6 + AOFF_task_thread + AOFF_thread_use_blkcommit], %g3

	/* Spitfire Errata #32 workaround */
	mov		PRIMARY_CONTEXT, %o4
	stxa		%g0, [%o4] ASI_DMMU
	membar		#Sync

	ldxa		[%o3] ASI_DTLB_TAG_READ, %o4

	/* Spitfire Errata #32 workaround */
	mov		PRIMARY_CONTEXT, %o5
	stxa		%g0, [%o5] ASI_DMMU
	membar		#Sync

	ldxa		[%o3] ASI_DTLB_DATA_ACCESS, %g0
	ldxa		[%o3] ASI_DTLB_DATA_ACCESS, %o5
	stxa		%o0, [%o2] ASI_DMMU
	stxa		%g1, [%o3] ASI_DTLB_DATA_ACCESS
	membar		#Sync
	add		%o3, (TLBTEMP_ENTSZ), %o3

	/* Spitfire Errata #32 workaround */
	mov		PRIMARY_CONTEXT, %g5
	stxa		%g0, [%g5] ASI_DMMU
	membar		#Sync

	ldxa		[%o3] ASI_DTLB_TAG_READ, %g5

	/* Spitfire Errata #32 workaround */
	mov		PRIMARY_CONTEXT, %g7
	stxa		%g0, [%g7] ASI_DMMU
	membar		#Sync

	ldxa		[%o3] ASI_DTLB_DATA_ACCESS, %g0
	ldxa		[%o3] ASI_DTLB_DATA_ACCESS, %g7
	stxa		%o1, [%o2] ASI_DMMU
	stxa		%g2, [%o3] ASI_DTLB_DATA_ACCESS
	membar		#Sync

	cmp		%g3, 0
	bne,pn		%xcc, copy_page_using_blkcommit
	 nop

	BRANCH_IF_ANY_CHEETAH(g3,o2,cheetah_copy_user_page)
	ba,pt		%xcc, spitfire_copy_user_page
	 nop

cheetah_copy_user_page:
	.globl		cheetah_copy_user_page_nop_1_6
cheetah_copy_user_page_nop_1_6:
	ldxa		[%g0] ASI_DCU_CONTROL_REG, %g3
	sethi		%uhi(DCU_PE), %o2
	sllx		%o2, 32, %o2
	or		%g3, %o2, %o2
	stxa		%o2, [%g0] ASI_DCU_CONTROL_REG	! Enable P-cache
	membar		#Sync

	sethi		%hi((PAGE_SIZE/64)-7), %o2	! A0 Group
	prefetch	[%o1 + 0x000], #one_read	! MS
	or		%o2, %lo((PAGE_SIZE/64)-7), %o2	! A1 Group
	prefetch	[%o1 + 0x040], #one_read	! MS
	prefetch	[%o1 + 0x080], #one_read	! MS Group
	prefetch	[%o1 + 0x0c0], #one_read	! MS Group
	ldd		[%o1 + 0x000], %f0		! MS Group
	prefetch	[%o1 + 0x100], #one_read	! MS Group
	ldd		[%o1 + 0x008], %f2		! AX
	prefetch	[%o1 + 0x140], #one_read	! MS Group
	ldd		[%o1 + 0x010], %f4		! AX
	prefetch	[%o1 + 0x180], #one_read	! MS Group
	fmovd		%f0, %f32			! FGA Group
	ldd		[%o1 + 0x018], %f6		! AX
	fmovd		%f2, %f34			! FGA Group
	ldd		[%o1 + 0x020], %f8		! MS
	fmovd		%f4, %f36			! FGA Group
	ldd		[%o1 + 0x028], %f10		! AX
	membar		#StoreStore			! MS
	fmovd		%f6, %f38			! FGA Group
	ldd		[%o1 + 0x030], %f12		! MS
	fmovd		%f8, %f40			! FGA Group
	ldd		[%o1 + 0x038], %f14		! AX
	fmovd		%f10, %f42			! FGA Group
	ldd		[%o1 + 0x040], %f16		! MS
1:	ldd		[%o1 + 0x048], %f2		! AX (Group)
	fmovd		%f12, %f44			! FGA
	ldd		[%o1 + 0x050], %f4		! MS
	fmovd		%f14, %f46			! FGA Group
	stda		%f32, [%o0] ASI_BLK_P		! MS
	ldd		[%o1 + 0x058], %f6		! AX
	fmovd		%f16, %f32			! FGA Group (8-cycle stall)
	ldd		[%o1 + 0x060], %f8		! MS
	fmovd		%f2, %f34			! FGA Group
	ldd		[%o1 + 0x068], %f10		! AX
	fmovd		%f4, %f36			! FGA Group
	ldd		[%o1 + 0x070], %f12		! MS
	fmovd		%f6, %f38			! FGA Group
	ldd		[%o1 + 0x078], %f14		! AX
	fmovd		%f8, %f40			! FGA Group
	ldd		[%o1 + 0x080], %f16		! AX
	prefetch	[%o1 + 0x180], #one_read	! MS
	fmovd		%f10, %f42			! FGA Group
	subcc		%o2, 1, %o2			! A0
	add		%o0, 0x40, %o0			! A1
	bne,pt		%xcc, 1b			! BR
	 add		%o1, 0x40, %o1			! A0 Group

	mov		5, %o2				! A0 Group
1:	ldd		[%o1 + 0x048], %f2		! AX
	fmovd		%f12, %f44			! FGA
	ldd		[%o1 + 0x050], %f4		! MS
	fmovd		%f14, %f46			! FGA Group
	stda		%f32, [%o0] ASI_BLK_P		! MS
	ldd		[%o1 + 0x058], %f6		! AX
	fmovd		%f16, %f32			! FGA Group (8-cycle stall)
	ldd		[%o1 + 0x060], %f8		! MS
	fmovd		%f2, %f34			! FGA Group
	ldd		[%o1 + 0x068], %f10		! AX
	fmovd		%f4, %f36			! FGA Group
	ldd		[%o1 + 0x070], %f12		! MS
	fmovd		%f6, %f38			! FGA Group
	ldd		[%o1 + 0x078], %f14		! AX
	fmovd		%f8, %f40			! FGA Group
	ldd		[%o1 + 0x080], %f16		! MS
	fmovd		%f10, %f42			! FGA Group
	subcc		%o2, 1, %o2			! A0
	add		%o0, 0x40, %o0			! A1
	bne,pt		%xcc, 1b			! BR
	 add		%o1, 0x40, %o1			! A0 Group

	ldd		[%o1 + 0x048], %f2		! AX
	fmovd		%f12, %f44			! FGA
	ldd		[%o1 + 0x050], %f4		! MS
	fmovd		%f14, %f46			! FGA Group
	stda		%f32, [%o0] ASI_BLK_P		! MS
	ldd		[%o1 + 0x058], %f6		! AX
	fmovd		%f16, %f32			! FGA Group (8-cycle stall)
	ldd		[%o1 + 0x060], %f8		! MS
	fmovd		%f2, %f34			! FGA Group
	ldd		[%o1 + 0x068], %f10		! AX
	fmovd		%f4, %f36			! FGA Group
	ldd		[%o1 + 0x070], %f12		! MS
	fmovd		%f6, %f38			! FGA Group
	add		%o0, 0x40, %o0			! A0
	ldd		[%o1 + 0x078], %f14		! AX
	fmovd		%f8, %f40			! FGA Group
	fmovd		%f10, %f42			! FGA Group
	fmovd		%f12, %f44			! FGA Group
	fmovd		%f14, %f46			! FGA Group
	stda		%f32, [%o0] ASI_BLK_P		! MS
	.globl		cheetah_copy_user_page_nop_2_3
cheetah_copy_user_page_nop_2_3:
	mov		PRIMARY_CONTEXT, %o2
	stxa		%g0, [%o2] ASI_DMMU		! Flush P-cache
	stxa		%g3, [%g0] ASI_DCU_CONTROL_REG	! Disable P-cache
	ba,a,pt		%xcc, copy_user_page_continue

spitfire_copy_user_page:
	ldda		[%o1] ASI_BLK_P, %f0
	add		%o1, 0x40, %o1
	ldda		[%o1] ASI_BLK_P, %f16
	add		%o1, 0x40, %o1
	sethi		%hi(PAGE_SIZE), %o2
1:	TOUCH(f0, f2, f4, f6, f8, f10, f12, f14)
	ldda		[%o1] ASI_BLK_P, %f32
	stda		%f48, [%o0] ASI_BLK_P
	add		%o1, 0x40, %o1
	sub		%o2, 0x40, %o2
	add		%o0, 0x40, %o0
	TOUCH(f16, f18, f20, f22, f24, f26, f28, f30)
	ldda		[%o1] ASI_BLK_P, %f0
	stda		%f48, [%o0] ASI_BLK_P
	add		%o1, 0x40, %o1
	sub		%o2, 0x40, %o2
	add		%o0, 0x40, %o0
	TOUCH(f32, f34, f36, f38, f40, f42, f44, f46)
	ldda		[%o1] ASI_BLK_P, %f16
	stda		%f48, [%o0] ASI_BLK_P
	sub		%o2, 0x40, %o2
	add		%o1, 0x40, %o1
	cmp		%o2, PAGE_SIZE_REM
	bne,pt		%xcc, 1b
	 add		%o0, 0x40, %o0
#if (PAGE_SHIFT == 16) || (PAGE_SHIFT == 22)
	TOUCH(f0, f2, f4, f6, f8, f10, f12, f14)
	ldda		[%o1] ASI_BLK_P, %f32
	stda		%f48, [%o0] ASI_BLK_P
	add		%o1, 0x40, %o1
	sub		%o2, 0x40, %o2
	add		%o0, 0x40, %o0
	TOUCH(f16, f18, f20, f22, f24, f26, f28, f30)
	ldda		[%o1] ASI_BLK_P, %f0
	stda		%f48, [%o0] ASI_BLK_P
	add		%o1, 0x40, %o1
	sub		%o2, 0x40, %o2
	add		%o0, 0x40, %o0
	membar		#Sync
	stda		%f32, [%o0] ASI_BLK_P
	add		%o0, 0x40, %o0
	stda		%f0, [%o0] ASI_BLK_P
#else
	membar		#Sync
	stda		%f0, [%o0] ASI_BLK_P
	add		%o0, 0x40, %o0
	stda		%f16, [%o0] ASI_BLK_P
#endif
copy_user_page_continue:
	membar		#Sync
	VISExit

	mov		TLB_TAG_ACCESS, %o2
	stxa		%g5, [%o2] ASI_DMMU
	stxa		%g7, [%o3] ASI_DTLB_DATA_ACCESS
	membar		#Sync
	sub		%o3, (TLBTEMP_ENTSZ), %o3
	stxa		%o4, [%o2] ASI_DMMU
	stxa		%o5, [%o3] ASI_DTLB_DATA_ACCESS
	membar		#Sync
	rdpr		%pstate, %g3
	jmpl		%o7 + 0x8, %g0
	 wrpr		%g3, PSTATE_IE, %pstate

copy_page_using_blkcommit:
	membar		#LoadStore | #StoreStore | #StoreLoad
	ldda		[%o1] ASI_BLK_P, %f0
	add		%o1, 0x40, %o1
	ldda		[%o1] ASI_BLK_P, %f16
	add		%o1, 0x40, %o1
	sethi		%hi(PAGE_SIZE), %o2
1:	TOUCH(f0, f2, f4, f6, f8, f10, f12, f14)
	ldda		[%o1] ASI_BLK_P, %f32
	stda		%f48, [%o0] ASI_BLK_COMMIT_P
	add		%o1, 0x40, %o1
	sub		%o2, 0x40, %o2
	add		%o0, 0x40, %o0
	TOUCH(f16, f18, f20, f22, f24, f26, f28, f30)
	ldda		[%o1] ASI_BLK_P, %f0
	stda		%f48, [%o0] ASI_BLK_COMMIT_P
	add		%o1, 0x40, %o1
	sub		%o2, 0x40, %o2
	add		%o0, 0x40, %o0
	TOUCH(f32, f34, f36, f38, f40, f42, f44, f46)
	ldda		[%o1] ASI_BLK_P, %f16
	stda		%f48, [%o0] ASI_BLK_COMMIT_P
	sub		%o2, 0x40, %o2
	add		%o1, 0x40, %o1
	cmp		%o2, PAGE_SIZE_REM
	bne,pt		%xcc, 1b
	 add		%o0, 0x40, %o0
#if (PAGE_SHIFT == 16) || (PAGE_SHIFT == 22)
	TOUCH(f0, f2, f4, f6, f8, f10, f12, f14)
	ldda		[%o1] ASI_BLK_P, %f32
	stda		%f48, [%o0] ASI_BLK_COMMIT_P
	add		%o1, 0x40, %o1
	sub		%o2, 0x40, %o2
	add		%o0, 0x40, %o0
	TOUCH(f16, f18, f20, f22, f24, f26, f28, f30)
	ldda		[%o1] ASI_BLK_P, %f0
	stda		%f48, [%o0] ASI_BLK_COMMIT_P
	add		%o1, 0x40, %o1
	sub		%o2, 0x40, %o2
	add		%o0, 0x40, %o0
	membar		#Sync
	stda		%f32, [%o0] ASI_BLK_COMMIT_P
	add		%o0, 0x40, %o0
	ba,pt		%xcc, copy_user_page_continue
	 stda		%f0, [%o0] ASI_BLK_COMMIT_P
#else
	membar		#Sync
	stda		%f0, [%o0] ASI_BLK_COMMIT_P
	add		%o0, 0x40, %o0
	ba,pt		%xcc, copy_user_page_continue
	 stda		%f16, [%o0] ASI_BLK_COMMIT_P
#endif

	.align		32
	.globl		_clear_page
	.type		_clear_page,@function
_clear_page:	/* %o0=dest */
	VISEntryHalf
	ba,pt		%xcc, clear_page_common
	 clr		%o4

	.align		32
	.globl		clear_user_page
	.type		clear_user_page,@function
clear_user_page:	/* %o0=dest, %o1=vaddr */
	VISEntryHalf
	sethi		%hi(PAGE_SIZE), %g3
	sub		%o0, %g4, %g1
	and		%o1, %g3, %o0
	mov		TLB_TAG_ACCESS, %o2
	sethi		%uhi(_PAGE_VALID | _PAGE_SZBITS), %g3
	sethi		%hi(TLBTEMP_BASE), %o3
	sllx		%g3, 32, %g3
	or		%g3, (_PAGE_CP | _PAGE_CV | _PAGE_P | _PAGE_L | _PAGE_W), %g3
	or		%g1, %g3, %g1
	add		%o0, %o3, %o0
#define FIX_INSN_2	0x96102068 /* mov (13 << 3), %o3 */
cheetah_patch_2:
	mov		TLBTEMP_ENT2, %o3
	rdpr		%pstate, %g3
	wrpr		%g3, PSTATE_IE, %pstate

	/* Spitfire Errata #32 workaround */
	mov		PRIMARY_CONTEXT, %g5
	stxa		%g0, [%g5] ASI_DMMU
	membar		#Sync

	ldxa		[%o3] ASI_DTLB_TAG_READ, %g5

	/* Spitfire Errata #32 workaround */
	mov		PRIMARY_CONTEXT, %g7
	stxa		%g0, [%g7] ASI_DMMU
	membar		#Sync

	ldxa		[%o3] ASI_DTLB_DATA_ACCESS, %g0
	ldxa		[%o3] ASI_DTLB_DATA_ACCESS, %g7
	stxa		%o0, [%o2] ASI_DMMU
	stxa		%g1, [%o3] ASI_DTLB_DATA_ACCESS
	membar		#Sync

	mov		1, %o4

clear_page_common:
	membar		#StoreLoad | #StoreStore | #LoadStore	! LSU	Group
	fzero		%f0				! FPA	Group
	sethi		%hi(PAGE_SIZE/256), %o1		! IEU0
	fzero		%f2				! FPA	Group
	or		%o1, %lo(PAGE_SIZE/256), %o1	! IEU0
	faddd		%f0, %f2, %f4			! FPA	Group
	fmuld		%f0, %f2, %f6			! FPM
	faddd		%f0, %f2, %f8			! FPA	Group
	fmuld		%f0, %f2, %f10			! FPM

	faddd		%f0, %f2, %f12			! FPA	Group
	fmuld		%f0, %f2, %f14			! FPM
1:	stda		%f0, [%o0 + %g0] ASI_BLK_P	! Store	Group
	add		%o0, 0x40, %o0			! IEU0
	stda		%f0, [%o0 + %g0] ASI_BLK_P	! Store	Group
	add		%o0, 0x40, %o0			! IEU0
	stda		%f0, [%o0 + %g0] ASI_BLK_P	! Store	Group

	add		%o0, 0x40, %o0			! IEU0	Group
	stda		%f0, [%o0 + %g0] ASI_BLK_P	! Store	Group
	subcc		%o1, 1, %o1			! IEU1
	bne,pt		%icc, 1b			! CTI
	 add		%o0, 0x40, %o0			! IEU0	Group
	membar		#Sync				! LSU	Group
	VISExitHalf

	brnz,pt		%o4, 1f
	 nop

	retl
	 nop

1:
	stxa		%g5, [%o2] ASI_DMMU
	stxa		%g7, [%o3] ASI_DTLB_DATA_ACCESS
	membar		#Sync
	jmpl		%o7 + 0x8, %g0
	 wrpr		%g3, 0x0, %pstate

	.globl		cheetah_patch_pgcopyops
cheetah_patch_pgcopyops:
	sethi		%hi(FIX_INSN_1), %g1
	or		%g1, %lo(FIX_INSN_1), %g1
	sethi		%hi(cheetah_patch_1), %g2
	or		%g2, %lo(cheetah_patch_1), %g2
	stw		%g1, [%g2]
	flush		%g2
	sethi		%hi(FIX_INSN_2), %g1
	or		%g1, %lo(FIX_INSN_2), %g1
	sethi		%hi(cheetah_patch_2), %g2
	or		%g2, %lo(cheetah_patch_2), %g2
	stw		%g1, [%g2]
	flush		%g2
	retl
	 nop

#undef FIX_INSN1
#undef FIX_INSN2
#undef PAGE_SIZE_REM
