/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * arch/sh5/lib/checksum.S
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli, Stefano D'Andrea
 *
 */

/*
 *
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		IP/TCP/UDP checksumming routines
 *
 * Authors:	Jorge Cwik, <jorge@laser.satlink.net>
 *		Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *		Tom May, <ftom@netcom.com>
 *              Pentium Pro/II routines:
 *              Alexander Kjeldaas <astor@guardian.no>
 *              Finn Arne Gangstad <finnag@guardian.no>
 *		Lots of code moved from tcp.c and ip.c; see those files
 *		for more names.
 *
 * Changes:     Ingo Molnar, converted csum_partial_copy() to 2.1 exception
 *			     handling.
 *		Andi Kleen,  add zeroing on error
 *                   converted to pure assembler
 *
 * SuperH version:  Copyright (C) 1999  Niibe Yutaka
 *
 * SH-5 version:  Copyright (C) 2000  Paolo Alberelli, Stefano D'Andrea
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <asm/errno.h>
#include <asm/registers.h>

	.section	.text64, "ax"
/*	
 * unsigned int csum_partial(const unsigned char *buf,
 *			     int 	          len,
 *                           unsigned int         sum);
 *
 *
 * computes a partial checksum, e.g. for TCP/UDP fragments
 */

/*
 * SH-5 ABI convention:
 *    Input parameters:
 *    		r2  = *buf
 *    		r3  =  len
 *    		r4  =  sum	so far computed checksum (may be zero)
 *    return value must be in:
 *              r2		returned checksum
 */

	/*
	 * Experiments with Ethernet and SLIP connections show that buff
	 * is aligned on either a 2-byte or 4-byte boundary.  We get at
	 * least a twofold speedup on 486 and Pentium if it is 4-byte aligned.
	 * Fortunately, it is easy to convert 2-byte alignment to 4-byte
	 * alignment for the unrolled loop.
	 */

#ifndef NOTDEF  
	/* This version of _csum_partial is an easy but unefficient
	 * implementation. 
	 * It's mainteined only for historical reasons.	
	 */
	.global csum_partial
csum_partial:
	_ptar		.byte_footer, t1	/* t1 = .byte_footer         */
	_ptar		.add_short_loop, t2	/* t2 = checksum eval loop   */
	_ptar		.exiting, t3		/* t3 = add carry...	     */

	movi		2, r1

	/* we assume buf short aligned */
	/* Short must be checksummed   */
.add_short_loop:
	bgt		r1, r3, t1		/* Size = 1 or 0 (remind)    */

	ld.uw		r2, 0, r7		/* r7 = word to be checksumed*/
	add		r4, r7, r4

	addi		r2, 2, r2		/* move buf forward...       */
	addi		r3, -2, r3		/* decrement len             */
	blink		t2, ZERO		/* goto .add_short_loop      */
	
.byte_footer:
	/* still one byte to be checksummed ? */
	xor		r7, r7, r7
	beqi		r3, 0, t3
	ld.ub		r2, 0, r7		/* r7 = last byte...	     */
#ifndef __LITTLE_ENDIAN__
	shlli		r7, 8, r7
#endif

.exiting:
	add		r4, r7, r2
	
	ptabs		r18, t0
	blink		t0, ZERO

#else  /* NOTDEF ---------------------------------------------------- */

	.global csum_partial
csum_partial:
	movi		32, r8			/* r8 = sizeof(8 * int)   */
	_ptar		.byte_footer, t1	/* t1 = .byte_footer      */
	_ptar		.word_footer, t2	/* t2 = .word_footer      */
	_ptar		.long_footer, t3	/* t3 = .long_footer      */
	_ptar		.exit, t4		/* t4 = exit point        */
	_ptar		.long_aligned, t0	/* t0 = .long_aligned	  */
	or		r2, ZERO, r5		/* r5 = buffer pointer    */
	or		r3, ZERO, r6		/* r6 = original length   */
	andi		r2, 2, r7 		
	beq		r7, ZERO, t0		/* It's buf long aligned */

	/* we assume buf short aligned */

	beqi		r3, 1, t1		/* Size = 1 */

	/* Short must be checksummed */
	ld.uw		r2, 0, r7		/* r7 = word to be checksumed*/
	add		r4, r7, r4

	or		ZERO, ZERO, r7

	mshflo.l	r4, r7, r7
	shlri		r4, 32, r4
	add		r4, r7, r4		/* Add eventual "carry" */
	addi		r6, -2, r6
	addi		r5, 2, r5		/* r5 is now long aligned */
	beq		r6, ZERO, t4		/* Exit if done */
	
	or		ZERO, ZERO, r7		/* Clean up r7 */
	
.long_aligned:
	bgt		r8, r6, t3
	
	/* 8 Longs to be checksummed */
	ld.l		r5, 0, r7		/* r7 = data to be checksummed*/
	add		r4, r7, r4
	or		ZERO, ZERO, r7
	mshflo.l	r4, r7, r7
	shlri		r4, 32, r4
	add		r4, r7, r4		/* Add eventual "carry" */
	ld.l		r5, 4, r7		/* r7 = data to be checksummed*/
	add		r4, r7, r4
	or		ZERO, ZERO, r7


	mshflo.l	r4, r7, r7
	shlri		r4, 32, r4
	add		r4, r7, r4		/* Add eventual "carry" */
	ld.l		r5, 8, r7		/* r7 = data to be checksummed*/
	add		r4, r7, r4
	or		ZERO, ZERO, r7
	mshflo.l	r4, r7, r7
	shlri		r4, 32, r4
	add		r4, r7, r4		/* Add eventual "carry" */

	ld.l		r5, 12, r7		/* r7 = data to be checksummed*/
	add		r4, r7, r4
	or		ZERO, ZERO, r7
	mshflo.l	r4, r7, r7
	shlri		r4, 32, r4
	add		r4, r7, r4		/* Add eventual "carry" */


	ld.l		r5, 16, r7		/* r7 = data to be checksummed*/
	add		r4, r7, r4
	or		ZERO, ZERO, r7
	mshflo.l	r4, r7, r7
	shlri		r4, 32, r4
	add		r4, r7, r4		/* Add eventual "carry" */

	ld.l		r5, 20, r7		/* r7 = data to be checksummed*/
	add		r4, r7, r4
	or		ZERO, ZERO, r7
	mshflo.l	r4, r7, r7
	shlri		r4, 32, r4
	add		r4, r7, r4		/* Add eventual "carry" */


	ld.l		r5, 24, r7		/* r7 = data to be checksummed*/
	add		r4, r7, r4
	or		ZERO, ZERO, r7
	mshflo.l	r4, r7, r7
	shlri		r4, 32, r4
	add		r4, r7, r4		/* Add eventual "carry" */


	ld.l		r5, 28, r7		/* r7 = data to be checksummed*/
	add		r4, r7, r4
	or		ZERO, ZERO, r7
	mshflo.l	r4, r7, r7
	shlri		r4, 32, r4
	add		r4, r7, r4		/* Add eventual "carry" */


	sub		r6, r8, r6
	add		r5, r8, r5
	blink		t0, ZERO
	
.long_footer:
	movi		4, r8
	bgt		r8, r6, t2
	
	/* Long to be checksummed */
	ld.l		r5, 0, r7		/* r7 = data to be checksummed*/
	add		r4, r7, r4
	or		ZERO, ZERO, r7
	mshflo.l	r4, r7, r7
	shlri		r4, 32, r4
	add		r4, r7, r4		/* Add eventual "carry" */


	sub		r6, r8, r6
	add		r5, r8, r5
	blink		t3, ZERO
	
.word_footer:
	movi		2, r8
	bgt		r8, r6, t1

	/* Short to be checksummed */
	ld.uw		r5, 0, r7		/* r7 = data to be checksummed*/
	add		r4, r7, r4
	or		ZERO, ZERO, r7
	mshflo.l	r4, r7, r7
	shlri		r4, 32, r4
	add		r4, r7, r4		/* Add eventual "carry" */


	sub		r6, r8, r6
	add		r5, r8, r5
	
.byte_footer:
	beqi		r6, 0, t4
	/* Short to be checksummed */
	ld.ub		r5, 0, r7		/* r7 = data to be checksummed*/

#ifndef __LITTLE_ENDIAN__
	shlli		r7, 8, r7
#endif

	add		r4, r7, r4
	or		ZERO, ZERO, r7
	mshflo.l	r4, r7, r7
	shlri		r4, 32, r4
	add		r4, r7, r4		/* Add eventual "carry" */


.exit:
	or		r4, ZERO, r2

	ptabs		r18, t0
	blink		t0, ZERO

#endif  /*NOTDEF*/

/*
 * unsigned int csum_partial_copy_generic (const char *src,
 *					         char *dst,
 *						 int len, 
 *						 int sum,
 *					         int *src_err_ptr,
 *					         int *dst_err_ptr)
 *  
 *
 *
 * Copy from ds while checksumming, otherwise like csum_partial
 *
 * The macros SRC and DST specify the type of access for the instruction.
 * thus we can call a custom exception handler for all access types.
 *
 * FIXME: could someone double-check whether I haven't mixed up some SRC and
 *	  DST definitions? It's damn hard to trigger all cases.  I hope I got
 *	  them all but there's no guarantee.
 */

/*
 * SH-5 ABI convention:
 *    Input parameters:
 *    		r2  = const char  *src
 *    		r3  = char	  *dst
 *    		r4  = int          len
 *    		r5  = int          sum   so far computed checksum (may be zero)
 *    		r6  = int         *src_err_ptr
 *    		r7  = int         *dst_err_ptr
 *    return value must be in:
 *              r2	returned checksum
 */

#ifndef NOTDEF  
	/* 
	** This version of _csum_partial_copy_generic is an easy but
	** unefficient implementation. 
	** It's mainteined only for historical reasons.	
	*/
	.global csum_partial_copy_generic
csum_partial_copy_generic:
	_ptar		.gc_byte_footer, t1	/* t1 = .byte_footer         */
	_ptar		.gc_add_short_loop, t2	/* t2 = checksum eval loop   */
	_ptar		.gc_exiting, t3		/* t3 = add carry...	     */

	or		r2, ZERO, r20		/* r20 = source pointer      */ 
	or		r3, ZERO, r21		/* r21 = destination pointer */ 

	movi		2, r1

	/* we assume buf short aligned */
	/* Short must be checksummed   */
.gc_add_short_loop:

	bgt		r1, r4, t1		/* Size = 1 or 0 (remind)    */

.src_err_1:
	ld.uw		r20, 0, r8		/* r8 = word to be checksum. */
	st.w		r21, 0, r8		/* fill data into DST */

	add		r5, r8, r5

	addi		r20, 2, r20		/* move SRC forward...       */
	addi		r21, 2, r21		/* move DST forward...       */
	addi		r4, -2, r4		/* decrement len             */
	blink		t2, ZERO		/* goto .gc_add_short_loop   */
	
.gc_byte_footer:
	/* still one byte to be checksummed ? */
	xor		r8, r8, r8
	beqi		r4, 0, t3

.src_err_2:
	ld.ub		r20, 0, r8		/* r8 = last byte...	     */
	st.b		r21, 0, r8
#ifndef __LITTLE_ENDIAN__
	shlli		r8, 8, r8
#endif

.gc_exiting:
	add		r5, r8, r2
	
	ptabs		r18, t0
	blink		t0, ZERO

	.section	.fixup, "ax"

_csum_partial_copy_generic_dst_err:
	movi		-(EFAULT), r8		/* r8 = EFAULT reply */
	st.l		r7, 0, r8		/* *DST_ERR = -EFAULT */

	/* Quiet exit */
	or		r5, ZERO, r2

	ptabs		r18, t0
	blink		t0, ZERO

_csum_partial_copy_generic_src_err:
	movi		-(EFAULT), r8		/* r8 = EFAULT reply */
	ld.l		r6, 0, r8		/* *SRC_ERR = -EFAULT */

	/*
	 * Now reset the DST buffer.	
	 * r20 points to the next DST byte.
	 * r3 points to the first DST byte.
	 */
	_ptar		.quiet_exit, t0
	_ptar		.src_err_loop, t1
	beq		r20, r3, t0

.src_err_loop:
	addi		r20, -1, r20
	ld.b		r20, 0, ZERO
	bne		r20, r3, t1

	/* Quiet exit */
.quiet_exit:
	or		r5, ZERO, r2

	ptabs		r18, t0
	blink		t0, ZERO

	.section	__ex_table, "a"

	.global asm_checksum_start	/* Just a marker */
asm_checksum_start:
	.long	.src_err_1, _csum_partial_copy_generic_src_err
	.long	.src_err_2, _csum_partial_copy_generic_src_err
	.long	.src_err_1+4, _csum_partial_copy_generic_dst_err
	.long	.src_err_2+4, _csum_partial_copy_generic_dst_err
	.global asm_checksum_end	/* Just a marker */
asm_checksum_end:

#else  /* NOTDEF -------------------------------------------------------- */

	.global csum_partial_copy_generic
csum_partial_copy_generic:

	movi		32, r27			/* r27 = sizeof(8 * int) */
	_ptar		.byte_footer_gc, t1	/* t1 = .byte_footer_gc */
	_ptar		.word_footer_gc, t2	/* t2 = .word_footer_gc */
	_ptar		.long_footer_gc, t3	/* t3 = .long_footer_gc */
	_ptar		.exit_gc, t4		/* t4 = exit point */

	or		r2, ZERO, r24		/* r24 = original SRC pointer */
	or		r3, ZERO, r20		/* r20 = original DST pointer */
	or		r4, ZERO, r25		/* r25 = original length */
	_ptar		.long_aligned_gc, t0
	andi		r2, 2, r26 		/* check if source it's  */
	beq		r26, ZERO, t0		/* long aligned 	 */

	/* It's short aligned */

	beqi		r4, 1, t1		/* Size = 1 */

	/* Short must be checksummed */
.src_err_1:
	ld.w		r2, 0, r26		/* r26: data to be checksummed*/
	st.w		r3, 0, r26		/* fill data into DST */
	add		r5, r26, r5
	or		ZERO, ZERO, r26
	mshflo.l	r5, r26, r26
	shlri		r5, 32, r5
	add		r5, r26, r5		/* Add eventual "carry" */
	
	addi		r25, -2, r25
	addi		r24, 2, r24		/* r24 is now long aligned */
	addi		r20, 2, r20		/* r20 it's now long aligned */
	beq		r25, ZERO, t4		/* Exit if done */
	
	or		ZERO, ZERO, r26		/* Clean up r26 */
	
.long_aligned_gc:
	bgt		r27, r25, t3
	
	/* 8 Longs to be checksummed */
.src_err_2:
	ld.l		r24, 0, r26		/* r26: data to be checksummed*/
	st.l		r20, 0, r26		/* fill data into DST */
	add		r5, r26, r5
	or		ZERO, ZERO, r26
	mshflo.l	r5, r26, r26
	shlri		r5, 32, r5
	add		r5, r26, r5		/* Add eventual "carry" */

.src_err_3:
	ld.l		r24, 4, r26		/* r26: data to be checksummed*/
	st.l		r20, 4, r26		/* fill data into DST */
	add		r5, r26, r5
	or		ZERO, ZERO, r26
	mshflo.l	r5, r26, r26
	shlri		r5, 32, r5
	add		r5, r26, r5		/* Add eventual "carry" */

.src_err_4:
	ld.l		r24, 8, r26		/* r26: data to be checksummed*/
	st.l		r20, 8, r26		/* fill data into DST */
	add		r5, r26, r5
	or		ZERO, ZERO, r26
	mshflo.l	r5, r26, r26
	shlri		r5, 32, r5
	add		r5, r26, r5		/* Add eventual "carry" */

.src_err_5:
	ld.l		r24, 12, r26		/* r26: data to be checksummed*/
	st.l		r20, 12, r26		/* fill data into DST */
	add		r5, r26, r5
	or		ZERO, ZERO, r26
	mshflo.l	r5, r26, r26
	shlri		r5, 32, r5
	add		r5, r26, r5		/* Add eventual "carry" */

.src_err_6:
	ld.l		r24, 16, r26		/* r26: data to be checksummed*/
	st.l		r20, 16, r26		/* fill data into DST */
	add		r5, r26, r5
	or		ZERO, ZERO, r26
	mshflo.l	r5, r26, r26
	shlri		r5, 32, r5
	add		r5, r26, r5		/* Add eventual "carry" */

.src_err_7:
	ld.l		r24, 20, r26		/* r26: data to be checksummed*/
	st.l		r20, 20, r26		/* fill data into DST */
	add		r5, r26, r5
	or		ZERO, ZERO, r26
	mshflo.l	r5, r26,r26
	shlri 		r5, 32, r5
	add		r5, r26, r5		/* Add eventual "carry" */

.src_err_8:
	ld.l		r24, 24, r26		/* r26: data to be checksummed*/
	st.l		r20, 24, r26		/* fill data into DST */
	add		r5, r26, r5
	or		ZERO, ZERO, r26
	mshflo.l	r5, r26, r26
	shlri		r5, 32, r5
	add		r5, r26, r5		/* Add eventual "carry" */

.src_err_9:
	ld.l		r24, 28, r26		/* r26: data to be checksummed*/
	st.l		r20, 28, r26		/* fill data into DST */
	add		r5, r26, r5
	or		ZERO, ZERO, r26
	mshflo.l	r5, r26, r26
	shlri		r5, 32, r5
	add		r5, r26, r5		/* Add eventual "carry" */

	sub		r25, r27, r25
	add		r24, r27, r24
	add		r20, r27, r20
	blink		t0, ZERO
	
.long_footer_gc:
	movi		4, r27
	bgt		r27, r25, t2
	
	/* Long to be checksummed */
.src_err_10:
	ld.l		r24, 0, r26		/* r26: data to be checksummed*/
	st.l		r20, 0, r26		/* fill data into DST */
	add		r5, r26, r5
	or		ZERO, ZERO, r26
	mshflo.l	r5, r26, r26
	shlri		r5, 32, r5
	add		r5, r26, r5		/* Add eventual "carry" */

	sub		r25, r27, r25
	add		r24, r27, r24
	add		r20, r27, r20
	blink		t3, ZERO
	
.word_footer_gc:
	movi		2, r27
	bgt		r27, r25, t1

	/* Short to be checksummed */
.src_err_11:
	ld.uw		r24, 0, r26		/* r26: data to be checksummed*/
	st.w		r20, 0, r26		/* fill data into DST */
	add		r5, r26, r5
	or		ZERO, ZERO, r26
	mshflo.l	r5, r26, r26
	shlri		r5, 32, r5
	add		r5, r26, r5		/* Add eventual "carry" */

	sub		r25, r27, r25
	add		r24, r27, r24
	add		r20, r27, r20
	
.byte_footer_gc:
	beqi		r25, 0, t4
	/* Short to be checksummed */
	ld.ub		r24, 0, r26		/* r26: data to be checksummed*/
.src_err_12:
	ld.ub		r24, 0, r26		/* r26: data to be checksummed*/
	st.b		r20, 0, r26		/* fill data into DST */

#ifndef __LITTLE_ENDIAN__
	shlli		r26, 8, r26
#endif

	add		r5, r26, r5
	or		ZERO, ZERO, r26
	mshflo.l	r5, r26, r26
	shlri		r5, 32, r5
	add		r5, r26, r5		/* Add eventual "carry" */

.exit_gc:
	or		r5, ZERO, r2

	ptabs		r18, t0
	blink		t0, ZERO

	.section	.fixup, "ax"

_csum_partial_copy_generic_dst_err:
	movi		-(EFAULT), r21		/* r21 = EFAULT reply */
	ld.l		r7, 0, r21

	/* Quiet exit */
	or		r5, ZERO, r2

	ptabs		r18, t0
	blink		t0, ZERO

_csum_partial_copy_generic_src_err:
	movi		-(EFAULT), r21		/* r21 = EFAULT reply */
	ld.l		r6, 0, r21

	/*
	 * Now reset the DST buffer.	
	 * r20 points to the next DST byte.
	 * r3 points to the first DST byte.
	 */
	_ptar		.quiet_exit, t0
	_ptar		.src_err_loop, t1
	beq		r20, r3, t0

.src_err_loop:
	addi		r20, -1, r20
	st.b		r20, 0, ZERO
	bne		r20, r3, t1

	/* Quiet exit */
.quiet_exit:
	or		r5, ZERO, r2

	ptabs		r18, t0
	blink		t0, ZERO

	.section	__ex_table, "a"

	.global asm_checksum_start	/* Just a marker */
asm_checksum_start:
	.long	.src_err_1, _csum_partial_copy_generic_src_err
	.long	.src_err_2, _csum_partial_copy_generic_src_err
	.long	.src_err_3, _csum_partial_copy_generic_src_err
	.long	.src_err_4, _csum_partial_copy_generic_src_err
	.long	.src_err_5, _csum_partial_copy_generic_src_err
	.long	.src_err_6, _csum_partial_copy_generic_src_err
	.long	.src_err_7, _csum_partial_copy_generic_src_err
	.long	.src_err_8, _csum_partial_copy_generic_src_err
	.long	.src_err_9, _csum_partial_copy_generic_src_err
	.long	.src_err_10, _csum_partial_copy_generic_src_err
	.long	.src_err_11, _csum_partial_copy_generic_src_err
	.long	.src_err_12, _csum_partial_copy_generic_src_err
	.long	.src_err_1+4, _csum_partial_copy_generic_dst_err
	.long	.src_err_2+4, _csum_partial_copy_generic_dst_err
	.long	.src_err_3+4, _csum_partial_copy_generic_dst_err
	.long	.src_err_4+4, _csum_partial_copy_generic_dst_err
	.long	.src_err_5+4, _csum_partial_copy_generic_dst_err
	.long	.src_err_6+4, _csum_partial_copy_generic_dst_err
	.long	.src_err_7+4, _csum_partial_copy_generic_dst_err
	.long	.src_err_8+4, _csum_partial_copy_generic_dst_err
	.long	.src_err_9+4, _csum_partial_copy_generic_dst_err
	.long	.src_err_10+4, _csum_partial_copy_generic_dst_err
	.long	.src_err_11+4, _csum_partial_copy_generic_dst_err
	.long	.src_err_12+4, _csum_partial_copy_generic_dst_err

	.global asm_checksum_end	/* Just a marker */
asm_checksum_end:
#endif  /* NOTDEF */
