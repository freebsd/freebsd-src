/*
 *  status_w.h
 *
 *
 * Copyright (C) 1992,1993,1994
 *                       W. Metzenthen, 22 Parker St, Ormond, Vic 3163,
 *                       Australia.  E-mail   billm@vaxc.cc.monash.edu.au
 * All rights reserved.
 *
 * This copyright notice covers the redistribution and use of the
 * FPU emulator developed by W. Metzenthen. It covers only its use
 * in the 386BSD, FreeBSD and NetBSD operating systems. Any other
 * use is not permitted under this copyright.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must include information specifying
 *    that source code for the emulator is freely available and include
 *    either:
 *      a) an offer to provide the source code for a nominal distribution
 *         fee, or
 *      b) list at least two alternative methods whereby the source
 *         can be obtained, e.g. a publically accessible bulletin board
 *         and an anonymous ftp site from which the software can be
 *         downloaded.
 * 3. All advertising materials specifically mentioning features or use of
 *    this emulator must acknowledge that it was developed by W. Metzenthen.
 * 4. The name of W. Metzenthen may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * W. METZENTHEN BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * The purpose of this copyright, based upon the Berkeley copyright, is to
 * ensure that the covered software remains freely available to everyone.
 *
 * The software (with necessary differences) is also available, but under
 * the terms of the GNU copyleft, for the Linux operating system and for
 * the djgpp ms-dos extender.
 *
 * W. Metzenthen   June 1994.
 *
 *
 * $FreeBSD$
 *
 */


#ifndef _STATUS_H_
#define _STATUS_H_


#ifdef LOCORE
#define	Const__(x)	$/**/x
#else
#define	Const__(x)	x
#endif

#define SW_Backward    	Const__(0x8000)	/* backward compatibility */
#define SW_C3		Const__(0x4000)	/* condition bit 3 */
#define SW_Top		Const__(0x3800)	/* top of stack */
#define SW_Top_Shift 	Const__(11)	/* shift for top of stack bits */
#define SW_C2		Const__(0x0400)	/* condition bit 2 */
#define SW_C1		Const__(0x0200)	/* condition bit 1 */
#define SW_C0		Const__(0x0100)	/* condition bit 0 */
#define SW_Summary     	Const__(0x0080)	/* exception summary */
#define SW_Stack_Fault	Const__(0x0040)	/* stack fault */
#define SW_Precision   	Const__(0x0020)	/* loss of precision */
#define SW_Underflow   	Const__(0x0010)	/* underflow */
#define SW_Overflow    	Const__(0x0008)	/* overflow */
#define SW_Zero_Div    	Const__(0x0004)	/* divide by zero */
#define SW_Denorm_Op   	Const__(0x0002)	/* denormalized operand */
#define SW_Invalid     	Const__(0x0001)	/* invalid operation */

#define SW_Exc_Mask     Const__(0x27f)	/* Status word exception bit mask */

#ifndef LOCORE

#define COMP_A_gt_B	1
#define COMP_A_eq_B	2
#define COMP_A_lt_B	3
#define COMP_No_Comp	4
#define COMP_Denormal   0x20
#define COMP_NaN	0x40
#define COMP_SNaN	0x80

#define setcc(cc) ({ \
  status_word &= ~(SW_C0|SW_C1|SW_C2|SW_C3); \
  status_word |= (cc) & (SW_C0|SW_C1|SW_C2|SW_C3); })

#endif				/* LOCORE */

#endif				/* _STATUS_H_ */
