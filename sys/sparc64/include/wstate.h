/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: BSDI: wstate.h,v 1.4 1997/09/18 13:05:51 torek Exp
 * $FreeBSD$
 */

#ifndef	_MACHINE_WSTATE_H_
#define	_MACHINE_WSTATE_H_

/*
 * Window state register bits.
 *
 * There really are no bits per se, just the two fields WSTATE.NORMAL
 * and WSTATE.OTHER.  The rest is up to software.
 *
 * We use WSTATE_NORMAL to represent user mode or kernel mode saves
 * (whichever is currently in effect) and WSTATE_OTHER to represent
 * user mode saves (only).
 *
 * We use the low bit to suggest 32-bit mode, with the next bit set
 * once we succeed in saving in some mode.  That is, if the WSTATE_ASSUME
 * bit is set, the spill or fill handler we use will be one that makes
 * an assumption about the proper window-save mode.  If the spill or
 * fill fails with an alignment fault, the spill or fill op should
 * take the `assume' bit away retry the instruction that caused the
 * spill or fill.  This will use the new %wstate, which will test for
 * which mode to use.  The alignment fault code helps us out here by
 * resuming the spill vector at offset +70, where we are allowed to
 * execute two instructions (i.e., write to %wstate and RETRY).
 *
 * If the ASSUME bit is not set when the alignment fault occurs, the
 * given stack pointer is hopelessly wrong (and the spill, if it is a
 * spill, should be done as a sort of "panic spill") -- so those two
 * instructions will be a branch sequence.
 *
 * Note that locore.s assumes this same bit layout (since the translation
 * from "bits" to "{spill,fill}_N_{normal,other}" is done in hardware).
 *
 * The value 0 is preferred for unknown to make it easy to start in
 * unknown state and continue in whichever state unknown succeeds in --
 * a successful "other" save, for instance, can just set %wstate to
 * ASSUMExx << USERSHIFT and thus leave the kernel state "unknown".
 *
 * We also need values for managing the somewhat tricky transition from
 * user to kernel and back, so we use the one remaining free bit to mean
 * "although this looks like kernel mode, the window(s) involved are
 * user windows and should be saved ASI_AIUP".  Everything else is
 * otherwise the same, but we need not bother with assumptions in this
 * mode (we expect it to apply to at most one window spill or fill),
 * i.e., WSTATE_TRANSITION can ignore WSTATE_ASSUME if it likes.
 */

#define	WSTATE_32BIT		1	/* if set, probably 32-bit mode */
#define	WSTATE_ASSUME		2	/* if set, assume 32 or 64 */
#define	WSTATE_TRANSITION	4	/* if set, force user window */

#define	WSTATE_TEST64		0	/* test, but anticipate 64-bit mode */
#define	WSTATE_TEST32		1	/* test, but anticipate 32-bit mode */
#define	WSTATE_ASSUME64		2	/* assume 64-bit mode */
#define	WSTATE_ASSUME32		3	/* assume 32-bit mode */

#define	WSTATE_MASK		7	/* for WSTATE.NORMAL */
#define	WSTATE_USERSHIFT	3	/* for WSTATE.OTHER / user */

#define	WSTATE_KERNEL		0	/* normal kernel wstate */

#endif /* !_MACHINE_WSTATE_H_ */
