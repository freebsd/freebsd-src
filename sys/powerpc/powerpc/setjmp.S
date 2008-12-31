/*	$FreeBSD: src/sys/powerpc/powerpc/setjmp.S,v 1.1.24.1 2008/11/25 02:59:29 kensmith Exp $  */
/*	from:	NetBSD: setjmp.S,v 1.1 1998/01/27 15:13:12 sakamoto Exp $  */
/*	from:	OpenBSD: setjmp.S,v 1.2 1996/12/28 06:22:18 rahnds Exp 	*/
/* kernel version of this file, does not have signal goop */
/* int setjmp(jmp_buf env) */

#define JMP_r1	0x04
#define JMP_r14	0x08
#define JMP_r15	0x0c
#define JMP_r16	0x10
#define JMP_r17	0x14
#define JMP_r18	0x18
#define JMP_r19	0x1c
#define JMP_r20	0x20
#define JMP_r21	0x24
#define JMP_r22	0x28
#define JMP_r23	0x2c
#define JMP_r24	0x30
#define JMP_r25	0x34
#define JMP_r26	0x38
#define JMP_r27	0x3c
#define JMP_r28	0x40
#define JMP_r29	0x44
#define JMP_r30	0x48
#define JMP_r31	0x4c
#define JMP_lr  0x50
#define JMP_cr  0x54
#define JMP_ctr	0x58
#define JMP_xer	0x5c
#define JMP_sig	0x60


.globl setjmp
setjmp:
	stw 31, JMP_r31(3)
	/* r1, r14-r30 */
	stw 1,  JMP_r1 (3)
	stw 14, JMP_r14(3)
	stw 15, JMP_r15(3)
	stw 16, JMP_r16(3)
	stw 17, JMP_r17(3)
	stw 18, JMP_r18(3)
	stw 19, JMP_r19(3)
	stw 20, JMP_r20(3)
	stw 21, JMP_r21(3)
	stw 22, JMP_r22(3)
	stw 23, JMP_r23(3)
	stw 24, JMP_r24(3)
	stw 25, JMP_r25(3)
	stw 26, JMP_r26(3)
	stw 27, JMP_r27(3)
	stw 28, JMP_r28(3)
	stw 29, JMP_r29(3)
	stw 30, JMP_r30(3)
	/* cr, lr, ctr, xer */
	mfcr 0
	stw 0, JMP_cr(3)
	mflr 0
	stw 0, JMP_lr(3)
	mfctr 0
	stw 0, JMP_ctr(3)
	mfxer 0
	stw 0, JMP_xer(3)
	/* f14-f31, fpscr */
	li 3, 0
	blr


.extern sigsetmask
.globl longjmp
longjmp:
	lwz 31, JMP_r31(3)
	/* r1, r14-r30 */
	lwz 1,  JMP_r1 (3)
	lwz 14, JMP_r14(3)
	lwz 15, JMP_r15(3)
	lwz 16, JMP_r16(3)
	lwz 17, JMP_r17(3)
	lwz 18, JMP_r18(3)
	lwz 19, JMP_r19(3)
	lwz 20, JMP_r20(3)
	lwz 21, JMP_r21(3)
	lwz 22, JMP_r22(3)
	lwz 23, JMP_r23(3)
	lwz 24, JMP_r24(3)
	lwz 25, JMP_r25(3)
	lwz 26, JMP_r26(3)
	lwz 27, JMP_r27(3)
	lwz 28, JMP_r28(3)
	lwz 29, JMP_r29(3)
	lwz 30, JMP_r30(3)
	/* cr, lr, ctr, xer */
	lwz 0, JMP_cr(3)
	mtcr 0
	lwz 0, JMP_lr(3)
	mtlr 0
	lwz 0, JMP_ctr(3)
	mtctr 0
	lwz 0, JMP_xer(3)
	mtxer 0
	/* f14-f31, fpscr */
	mr 3, 4
	blr
