#include <linux/config.h>

#include "entry.h"

/*
 * For ivt.s we want to access the stack virtually so we don't have to disable translation
 * on interrupts.
 *
 *  On entry:
 *	r1:	pointer to current task (ar.k6)
 */
#define MINSTATE_START_SAVE_MIN_VIRT								\
	dep r1=-1,r1,61,3;				/* r1 = current (virtual) */		\
(pUser)	mov ar.rsc=0;		/* set enforced lazy mode, pl 0, little-endian, loadrs=0 */	\
	;;											\
(pUser)	addl r22=IA64_RBS_OFFSET,r1;			/* compute base of RBS */		\
(pUser)	mov r24=ar.rnat;									\
(pKern) mov r1=sp;					/* get sp  */				\
	;;											\
(pUser)	addl r1=IA64_STK_OFFSET-IA64_PT_REGS_SIZE,r1;	/* compute base of memory stack */	\
(pUser)	mov r23=ar.bspstore;				/* save ar.bspstore */			\
	;;											\
(pKern) addl r1=-IA64_PT_REGS_SIZE,r1;			/* if in kernel mode, use sp (r12) */	\
(pUser)	mov ar.bspstore=r22;				/* switch to kernel RBS */		\
	;;											\
(pUser)	mov r18=ar.bsp;										\
(pUser)	mov ar.rsc=0x3;		/* set eager mode, pl 0, little-endian, loadrs=0 */		\

#define MINSTATE_END_SAVE_MIN_VIRT								\
	or r13=r13,r14;		/* make `current' a kernel virtual address */			\
	bsw.1;			/* switch back to bank 1 (must be last in insn group) */	\
	;;

/*
 * For mca_asm.S we want to access the stack physically since the state is saved before we
 * go virtual and don't want to destroy the iip or ipsr.
 */
#define MINSTATE_START_SAVE_MIN_PHYS								\
(pKern) movl sp=ia64_init_stack+IA64_STK_OFFSET-IA64_PT_REGS_SIZE;				\
(pUser)	mov ar.rsc=0;		/* set enforced lazy mode, pl 0, little-endian, loadrs=0 */	\
(pUser)	addl r22=IA64_RBS_OFFSET,r1;		/* compute base of register backing store */	\
	;;											\
(pUser)	mov r24=ar.rnat;									\
(pKern) dep r1=0,sp,61,3;				/* compute physical addr of sp	*/	\
(pUser)	addl r1=IA64_STK_OFFSET-IA64_PT_REGS_SIZE,r1;	/* compute base of memory stack */	\
(pUser)	mov r23=ar.bspstore;				/* save ar.bspstore */			\
(pUser)	dep r22=-1,r22,61,3;				/* compute kernel virtual addr of RBS */\
	;;											\
(pKern) addl r1=-IA64_PT_REGS_SIZE,r1;		/* if in kernel mode, use sp (r12) */		\
(pUser)	mov ar.bspstore=r22;			/* switch to kernel RBS */			\
	;;											\
(pUser)	mov r18=ar.bsp;										\
(pUser)	mov ar.rsc=0x3;		/* set eager mode, pl 0, little-endian, loadrs=0 */		\

#define MINSTATE_END_SAVE_MIN_PHYS								\
	or r12=r12,r14;		/* make sp a kernel virtual address */				\
	or r13=r13,r14;		/* make `current' a kernel virtual address */			\
	;;

#ifdef MINSTATE_VIRT
# define MINSTATE_START_SAVE_MIN	MINSTATE_START_SAVE_MIN_VIRT
# define MINSTATE_END_SAVE_MIN		MINSTATE_END_SAVE_MIN_VIRT
#endif

#ifdef MINSTATE_PHYS
# define MINSTATE_START_SAVE_MIN	MINSTATE_START_SAVE_MIN_PHYS
# define MINSTATE_END_SAVE_MIN		MINSTATE_END_SAVE_MIN_PHYS
#endif

/*
 * DO_SAVE_MIN switches to the kernel stacks (if necessary) and saves
 * the minimum state necessary that allows us to turn psr.ic back
 * on.
 *
 * Assumed state upon entry:
 *	psr.ic: off
 *	r31:	contains saved predicates (pr)
 *
 * Upon exit, the state is as follows:
 *	psr.ic: off
 *	 r2 = points to &pt_regs.r16
 *	 r8 = contents of ar.ccv
 *	 r9 = contents of ar.csd
 *	r10 = contents of ar.ssd
 *	r11 = FPSR_DEFAULT
 *	r12 = kernel sp (kernel virtual address)
 *	r13 = points to current task_struct (kernel virtual address)
 *	p15 = TRUE if psr.i is set in cr.ipsr
 *	predicate registers (other than p2, p3, and p15), b6, r3, r14, r15:
 *		preserved
 *
 * Note that psr.ic is NOT turned on by this macro.  This is so that
 * we can pass interruption state as arguments to a handler.
 */
#define DO_SAVE_MIN(COVER,SAVE_IFS,EXTRA)							\
	mov r29=cr.ipsr;									\
	mov r17=IA64_KR(CURRENT);		/* r1 = current (physical) */			\
	mov r20=r1;										\
	mov r27=ar.rsc;										\
	mov r25=ar.unat;									\
	mov r26=ar.pfs;										\
	mov r21=ar.fpsr;									\
	mov r28=cr.iip;										\
	COVER;											\
	;;											\
	invala;											\
	extr.u r16=r29,32,2;		/* extract psr.cpl */					\
	;;											\
	cmp.eq pKern,pUser=r0,r16;	/* are we in kernel mode already? (psr.cpl==0) */	\
	mov r1=r17; 										\
	/* switch from user to kernel RBS: */							\
	;;											\
	SAVE_IFS;										\
	MINSTATE_START_SAVE_MIN									\
	adds r17=2*L1_CACHE_BYTES,r1;		/* really: biggest cache-line size */		\
	adds r16=PT(CR_IPSR),r1;								\
	;;											\
	lfetch.fault.excl.nt1 [r17],L1_CACHE_BYTES;						\
	st8 [r16]=r29;		/* save cr.ipsr */						\
	;;											\
	lfetch.fault.excl.nt1 [r17];								\
	;;											\
	adds r16=PT(R8),r1;	/* initialize first base pointer */				\
	adds r17=PT(R9),r1;	/* initialize second base pointer */				\
	;;											\
.mem.offset 0,0; st8.spill [r16]=r8,16;								\
.mem.offset 8,0; st8.spill [r17]=r9,16;								\
	;;											\
.mem.offset 0,0; st8.spill [r16]=r10,24;							\
.mem.offset 8,0; st8.spill [r17]=r11,32;							\
	;;											\
	st8 [r16]=r28,8;	/* save cr.iip */						\
	mov r28=b0;          	/* rCRIIP=branch reg b0 */					\
(pKern)	mov r18=r0;		/* make sure r18 isn't NaT */					\
	mov r8=ar.ccv;                                                                          \
	mov r9=ar.csd;                                                                          \
	mov r10=ar.ssd;                                                                         \
	movl r11=FPSR_DEFAULT;   /* L-unit */                                                   \
	;;											\
	st8 [r16]=r30,16;	/* save cr.ifs */						\
	st8 [r17]=r25,16;	/* save ar.unat */						\
(pUser)	sub r18=r18,r22;	/* r18=RSE.ndirty*8 */						\
	;;											\
	st8 [r16]=r26,16;	/* save ar.pfs */						\
	st8 [r17]=r27,16;	/* save ar.rsc */						\
	tbit.nz p15,p0=r29,IA64_PSR_I_BIT							\
	;;			/* avoid RAW on r16 & r17 */					\
(pKern)	adds r16=16,r16;	/* skip over ar_rnat field */					\
(pKern)	adds r17=16,r17;	/* skip over ar_bspstore field */				\
(pUser)	st8 [r16]=r24,16;	/* save ar.rnat */						\
(pUser)	st8 [r17]=r23,16;	/* save ar.bspstore */					  	\
	;;											\
	st8 [r16]=r31,16;	/* save predicates */						\
	st8 [r17]=r28,16;	/* save b0 */							\
	shl r18=r18,16;		/* compute ar.rsc to be used for "loadrs" */			\
	;;											\
	st8 [r16]=r18,16;	/* save ar.rsc value for "loadrs" */				\
	st8.spill [r17]=r20,16;	/* save original r1 */						\
	;;											\
.mem.offset 0,0; st8.spill [r16]=r12,16;							\
.mem.offset 8,0; st8.spill [r17]=r13,16;							\
	cmp.eq pNonSys,pSys=r0,r0	/* initialize pSys=0, pNonSys=1 */			\
	;;											\
.mem.offset 0,0; st8 [r16]=r21,PT(R14)-PT(AR_FPSR); 	/* ar.fpsr */				\
.mem.offset 8,0; st8.spill [r17]=r15,PT(R3)-PT(R15);						\
	adds r12=-16,r1;	/* switch to kernel memory stack (with 16 bytes of scratch) */	\
	;;											\
	mov r13=IA64_KR(CURRENT);	/* establish `current' */				\
.mem.offset 0,0; st8.spill [r16]=r14,8;								\
	dep r14=-1,r0,61,3;									\
	;;											\
.mem.offset 0,0; st8.spill [r16]=r2,16;								\
.mem.offset 8,0; st8.spill [r17]=r3,16;								\
	adds r2=IA64_PT_REGS_R16_OFFSET,r1;							\
	;;											\
	EXTRA;											\
	movl r1=__gp;		/* establish kernel global pointer */				\
	;;											\
	MINSTATE_END_SAVE_MIN

/*
 * SAVE_REST saves the remainder of pt_regs (with psr.ic on).
 *
 * Assumed state upon entry:
 *	psr.ic: on
 *	r2:	points to &pt_regs.r16
 *	r3:	points to &pt_regs.r17
 *	r8:	contents of ar.ccv
 *	r9:	contents of ar.csd
 *	r10:	contents of ar.ssd
 *	r11:	FPSR_DEFAULT
 *
 * Registers r14 and r15 are guaranteed not to be touched by SAVE_REST.
 */
#define SAVE_REST				\
.mem.offset 0,0; st8.spill [r2]=r16,16;		\
.mem.offset 8,0; st8.spill [r3]=r17,16;		\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r18,16;		\
.mem.offset 8,0; st8.spill [r3]=r19,16;		\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r20,16;		\
.mem.offset 8,0; st8.spill [r3]=r21,16;		\
	mov r18=b6;				\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r22,16;		\
.mem.offset 8,0; st8.spill [r3]=r23,16;		\
	mov r19=b7;				\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r24,16;		\
.mem.offset 8,0; st8.spill [r3]=r25,16;		\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r26,16;		\
.mem.offset 8,0; st8.spill [r3]=r27,16;		\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r28,16;		\
.mem.offset 8,0; st8.spill [r3]=r29,16;		\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r30,16;		\
.mem.offset 8,0; st8.spill [r3]=r31,32;		\
	;;					\
	mov ar.fpsr=r11;	/* M-unit */	\
	st8 [r2]=r8,8;		/* ar.ccv */	\
	adds r24=PT(B6)-PT(F7),r3;		\
	;;					\
	stf.spill [r2]=f6,32;			\
	stf.spill [r3]=f7,32;			\
	;;					\
	stf.spill [r2]=f8,32;			\
	stf.spill [r3]=f9,32;			\
	;;					\
	stf.spill [r2]=f10;			\
	stf.spill [r3]=f11;			\
	adds r25=PT(B7)-PT(F11),r3;		\
	;;					\
	st8 [r24]=r18,16;	/* b6 */	\
	st8 [r25]=r19,16;	/* b7 */	\
	;;					\
	st8 [r24]=r9;		/* ar.csd */	\
	st8 [r25]=r10;		/* ar.ssd */	\
	;;

#define SAVE_MIN_WITH_COVER	DO_SAVE_MIN(cover, mov r30=cr.ifs,)
#define SAVE_MIN_WITH_COVER_R19	DO_SAVE_MIN(cover, mov r30=cr.ifs, mov r15=r19)
#define SAVE_MIN		DO_SAVE_MIN(     , mov r30=r0, )
