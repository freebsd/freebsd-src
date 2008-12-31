/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http:	//www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information:	 Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/sys/sun4v/cddl/t1_copy.S,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $")
	
#include <machine/asi.h>
#include <machine/asmacros.h>
#include <machine/ktr.h>
#include <machine/pstate.h>
#include <machine/trap.h>
#include <machine/tstate.h>
#include <machine/wstate.h>
#include <machine/hypervisorvar.h>

       .register %g2,#ignore
       .register %g3,#ignore
       .register %g6,#ignore
       .register %g7,#ignore
		
	
/*
 * This define is to align data for the unaligned source cases.
 * The data1, data2 and data3 is merged into data1 and data2.
 * The data3 is preserved for next merge.
 */
#define	ALIGN_DATA(data1, data2, data3, lshift, rshift, tmp)	\
	sllx	data1, lshift, data1				;\
	srlx	data2, rshift, tmp				;\
	or	data1, tmp, data1				;\
	sllx	data2, lshift, data2				;\
	srlx	data3, rshift, tmp				;\
	or	data2, tmp, data2
/*
 * This macro is to align the data. Basically it merges
 * data1 and data2 to form double word.
 */
#define	ALIGN_DATA_EW(data1, data2, lshift, rshift, tmp)	\
	sllx	data1, lshift, data1				;\
	srlx	data2, rshift, tmp				;\
	or	data1, tmp, data1



	
	
/*
 * DGDEF and DGDEF2 provide global data declarations.
 *
 * DGDEF provides a word aligned word of storage.
 *
 * DGDEF2 allocates "sz" bytes of storage with **NO** alignment.  This
 * implies this macro is best used for byte arrays.
 *
 * DGDEF3 allocates "sz" bytes of storage with "algn" alignment.
 */
#define DGDEF2(name, sz) \
        .section        ".data"	;  \
        .global name	;  \
        .type   name, @object ;  \
        .size   name, sz;  \
name:

#define DGDEF3(name, sz, algn) \
        .section        ".data"	;  \
        .align  algn	;  \
        .global name	;  \
        .type   name, @object ;  \
        .size   name, sz;  \
name:

#define DGDEF(name)     DGDEF3(name, 4, 4)
	
.align  4
DGDEF(hw_copy_limit_1)
.word   0x100
DGDEF(hw_copy_limit_2)
.word   0x200
DGDEF(hw_copy_limit_4)
.word   0x400
DGDEF(hw_copy_limit_8)
.word   0x400
.align  64
.section ".text"
	

#if defined(lint)

/*ARGSUSED*/
void
ovbcopy(const void *from, void *to, size_t count)
{}

#else	/* lint */

ENTRY(bcopy)
	tst	%o2			! check count
	bgu,a	%xcc, 1f		! nothing to do or bad arguments
	subcc	%o0, %o1, %o3		! difference of from and to address

	retl				! return
	  nop
1:
	bneg,a	%xcc, 2f
	neg	%o3			! if < 0, make it positive
2:	cmp	%o2, %o3		! cmp size and abs(from - to)
	bleu	%xcc, novbcopy		! if size <= abs(diff): use bcopy,
	  nop
	cmp	%o0, %o1		! compare from and to addresses
	blu	%xcc, ov_bkwd		! if from < to, copy backwards
	  nop
	!
	! Copy forwards.
	!
ov_fwd:
	ldub	[%o0], %o3		! read from address
	inc	%o0			! inc from address
	stb	%o3, [%o1]		! write to address
	deccc	%o2			! dec count
	bgu	%xcc, ov_fwd		! loop till done
	  inc	%o1			! inc to address

	retl				! return
	nop
	!
	! Copy backwards.
	!
ov_bkwd:
	deccc	%o2			! dec count
	ldub	[%o0 + %o2], %o3	! get byte at end of src
	bgu	%xcc, ov_bkwd		! loop till done
	  stb	%o3, [%o1 + %o2]	! delay slot, store at end of dst

	retl				! return
	nop
END(bcopy)

#endif	/* lint */


		
/*
 * Copy a block of storage - must not overlap (from + len <= to).
 */
ENTRY(novbcopy)

	save	%sp, -SA(MINFRAME), %sp

do_copy:
	cmp	%i2, 12			! for small counts
	blu	%xcc, bytecp		! just copy bytes
	  nop

	cmp	%i2, 128		! for less than 128 bytes
	blu,pn	%xcc, bcb_punt		! no block st/quad ld
	  nop
#if 0
	set	use_hw_bcopy, %o2
	ld	[%o2], %o2
	tst	%o2
	bz	bcb_punt
	  nop
#endif
	subcc	%i1, %i0, %i3
	bneg,a,pn %xcc, 1f
	  neg	%i3
1:
	/*
	 * Compare against 256 since we should be checking block addresses
	 * and (dest & ~63) - (src & ~63) can be 3 blocks even if
	 * src = dest + (64 * 3) + 63.
	 */
	cmp	%i3, 256
	blu,pn	%xcc, bcb_punt
	  nop

	/*
	 * Copy that reach here have at least 2 blocks of data to copy.
	 */
do_blockcopy:
	! Swap src/dst since the code below is memcpy code
	! and memcpy/bcopy have different calling sequences
	mov	%i1, %i5
	mov	%i0, %i1
	mov	%i5, %i0

	andcc	%i0, 0x3f, %i3		! is dst aligned on a 64 bytes
	bz	%xcc, chksrc		! dst is already double aligned
	  sub	%i3, 0x40, %i3
	neg	%i3			! bytes till dst 64 bytes aligned
	sub	%i2, %i3, %i2		! update i2 with new count

1:	ldub	[%i1], %i4
	stb	%i4, [%i0]
	inc	%i1
	deccc	%i3
	bgu	%xcc, 1b
	  inc	%i0

	! Now Destination is block (64 bytes) aligned
chksrc:
	andn	%i2, 0x3f, %i3		! %i3 count is multiple of block size
	sub	%i2, %i3, %i2		! Residue bytes in %i2

	wr	%g0, ASI_LDSTBI_P, %asi

	andcc	%i1, 0xf, %o2		! is src quadword aligned
	bz,pn	%xcc, blkcpy		! src offset in %o2
	nop
	cmp	%o2, 0x8
	bg	cpy_upper_double
	nop
	bl	cpy_lower_double
	nop

	! Falls through when source offset is equal to 8 i.e.
	! source is double word aligned.
	! In this case no shift/merge of data is required
	sub	%i1, %o2, %i1		! align the src at 16 bytes.
	andn	%i1, 0x3f, %l0		! %l0 has block aligned source
	prefetch [%l0+0x0], #one_read
	ldda	[%i1+0x0]%asi, %l2
loop0:
	ldda	[%i1+0x10]%asi, %l4
	prefetch [%l0+0x40], #one_read

	stxa	%l3, [%i0+0x0]%asi
	stxa	%l4, [%i0+0x8]%asi

	ldda	[%i1+0x20]%asi, %l2
	stxa	%l5, [%i0+0x10]%asi
	stxa	%l2, [%i0+0x18]%asi

	ldda	[%i1+0x30]%asi, %l4
	stxa	%l3, [%i0+0x20]%asi
	stxa	%l4, [%i0+0x28]%asi

	ldda	[%i1+0x40]%asi, %l2
	stxa	%l5, [%i0+0x30]%asi
	stxa	%l2, [%i0+0x38]%asi

	add	%l0, 0x40, %l0
	add	%i1, 0x40, %i1
	subcc	%i3, 0x40, %i3
	bgu,pt	%xcc, loop0
	  add	%i0, 0x40, %i0
	ba	blkdone
	add	%i1, %o2, %i1		! increment the source by src offset
					! the src offset was stored in %o2

cpy_lower_double:
	sub	%i1, %o2, %i1		! align the src at 16 bytes.
	sll	%o2, 3, %o0		! %o0 left shift
	mov	0x40, %o1
	sub	%o1, %o0, %o1		! %o1 right shift = (64 - left shift)
	andn	%i1, 0x3f, %l0		! %l0 has block aligned source
	prefetch [%l0+0x0], #one_read
	ldda	[%i1+0x0]%asi, %l2	! partial data in %l2 and %l3 has
					! complete data
loop1:
	ldda	[%i1+0x10]%asi, %l4	! %l4 has partial data for this read.
	ALIGN_DATA(%l2, %l3, %l4, %o0, %o1, %l6)	! merge %l2, %l3 and %l4
							! into %l2 and %l3
	prefetch [%l0+0x40], #one_read
	stxa	%l2, [%i0+0x0]%asi
	stxa	%l3, [%i0+0x8]%asi

	ldda	[%i1+0x20]%asi, %l2
	ALIGN_DATA(%l4, %l5, %l2, %o0, %o1, %l6)	! merge %l2 with %l5 and
	stxa	%l4, [%i0+0x10]%asi			! %l4 from previous read
	stxa	%l5, [%i0+0x18]%asi			! into %l4 and %l5

	! Repeat the same for next 32 bytes.

	ldda	[%i1+0x30]%asi, %l4
	ALIGN_DATA(%l2, %l3, %l4, %o0, %o1, %l6)
	stxa	%l2, [%i0+0x20]%asi
	stxa	%l3, [%i0+0x28]%asi

	ldda	[%i1+0x40]%asi, %l2
	ALIGN_DATA(%l4, %l5, %l2, %o0, %o1, %l6)
	stxa	%l4, [%i0+0x30]%asi
	stxa	%l5, [%i0+0x38]%asi

	add	%l0, 0x40, %l0
	add	%i1, 0x40, %i1
	subcc	%i3, 0x40, %i3
	bgu,pt	%xcc, loop1
	  add	%i0, 0x40, %i0
	ba	blkdone
	add	%i1, %o2, %i1		! increment the source by src offset
					! the src offset was stored in %o2

cpy_upper_double:
	sub	%i1, %o2, %i1		! align the src at 16 bytes.
	mov	0x8, %o0
	sub	%o2, %o0, %o0
	sll	%o0, 3, %o0		! %o0 left shift
	mov	0x40, %o1
	sub	%o1, %o0, %o1		! %o1 right shift = (64 - left shift)
	andn	%i1, 0x3f, %l0		! %l0 has block aligned source
	prefetch [%l0+0x0], #one_read
	ldda	[%i1+0x0]%asi, %l2	! partial data in %l3 for this read and
					! no data in %l2
loop2:
	ldda	[%i1+0x10]%asi, %l4	! %l4 has complete data and %l5 has
					! partial
	ALIGN_DATA(%l3, %l4, %l5, %o0, %o1, %l6)	! merge %l3, %l4 and %l5
							! into %l3 and %l4
	prefetch [%l0+0x40], #one_read
	stxa	%l3, [%i0+0x0]%asi
	stxa	%l4, [%i0+0x8]%asi

	ldda	[%i1+0x20]%asi, %l2
	ALIGN_DATA(%l5, %l2, %l3, %o0, %o1, %l6)	! merge %l2 and %l3 with
	stxa	%l5, [%i0+0x10]%asi			! %l5 from previous read
	stxa	%l2, [%i0+0x18]%asi			! into %l5 and %l2

	! Repeat the same for next 32 bytes.

	ldda	[%i1+0x30]%asi, %l4
	ALIGN_DATA(%l3, %l4, %l5, %o0, %o1, %l6)
	stxa	%l3, [%i0+0x20]%asi
	stxa	%l4, [%i0+0x28]%asi

	ldda	[%i1+0x40]%asi, %l2
	ALIGN_DATA(%l5, %l2, %l3, %o0, %o1, %l6)
	stxa	%l5, [%i0+0x30]%asi
	stxa	%l2, [%i0+0x38]%asi

	add	%l0, 0x40, %l0
	add	%i1, 0x40, %i1
	subcc	%i3, 0x40, %i3
	bgu,pt	%xcc, loop2
	  add	%i0, 0x40, %i0
	ba	blkdone
	add	%i1, %o2, %i1		! increment the source by src offset
					! the src offset was stored in %o2


	! Both Source and Destination are block aligned.
	! Do fast copy using ASI_LDSTBI_P
blkcpy:
	prefetch [%i1+0x0], #one_read
1:
	ldda	[%i1+0x0]%asi, %l0
	ldda	[%i1+0x10]%asi, %l2
	prefetch [%i1+0x40], #one_read

	stxa	%l0, [%i0+0x0]%asi
	ldda	[%i1+0x20]%asi, %l4
	ldda	[%i1+0x30]%asi, %l6

	stxa	%l1, [%i0+0x8]%asi
	stxa	%l2, [%i0+0x10]%asi
	stxa	%l3, [%i0+0x18]%asi
	stxa	%l4, [%i0+0x20]%asi
	stxa	%l5, [%i0+0x28]%asi
	stxa	%l6, [%i0+0x30]%asi
	stxa	%l7, [%i0+0x38]%asi

	add	%i1, 0x40, %i1
	subcc	%i3, 0x40, %i3
	bgu,pt	%xcc, 1b
	  add	%i0, 0x40, %i0

blkdone:
	tst	%i2
	bz,pt	%xcc, blkexit
	nop

residue:
	ldub	[%i1], %i4
	stb	%i4, [%i0]
	inc	%i1
	deccc	%i2
	bgu	%xcc, residue
	  inc	%i0

blkexit:
	membar	#Sync				! sync error barrier
	ret
	restore	%g0, 0, %o0

bcb_punt:
	!
	! use aligned transfers where possible
	!
	xor	%i0, %i1, %o4		! xor from and to address
	btst	7, %o4			! if lower three bits zero
	bz	aldoubcp		! can align on double boundary
	  nop	! assembler complaints about label

	xor	%i0, %i1, %o4		! xor from and to address
	btst	3, %o4			! if lower two bits zero
	bz	alwordcp		! can align on word boundary
	btst	3, %i0			! delay slot, from address unaligned?
	!
	! use aligned reads and writes where possible
	! this differs from wordcp in that it copes
	! with odd alignment between source and destnation
	! using word reads and writes with the proper shifts
	! in between to align transfers to and from memory
	! i0 - src address, i1 - dest address, i2 - count
	! i3, i4 - tmps for used generating complete word
	! i5 (word to write)
	! l0 size in bits of upper part of source word (US)
	! l1 size in bits of lower part of source word (LS = 32 - US)
	! l2 size in bits of upper part of destination word (UD)
	! l3 size in bits of lower part of destination word (LD = 32 - UD)
	! l4 number of bytes leftover after aligned transfers complete
	! l5 the number 32
	!
	mov	32, %l5			! load an oft-needed constant
	bz	align_dst_only
	  btst	3, %i1			! is destnation address aligned?
	clr	%i4			! clear registers used in either case
	bz	align_src_only
	  clr	%l0
	!
	! both source and destination addresses are unaligned
	!
1:					! align source
	ldub	[%i0], %i3		! read a byte from source address
	add	%i0, 1, %i0		! increment source address
	or	%i4, %i3, %i4		! or in with previous bytes (if any)
	btst	3, %i0			! is source aligned?
	add	%l0, 8, %l0		! increment size of upper source (US)
	bnz,a	1b
	  sll	%i4, 8, %i4		! make room for next byte

	sub	%l5, %l0, %l1		! generate shift left count (LS)
	sll	%i4, %l1, %i4		! prepare to get rest
	ld	[%i0], %i3		! read a word
	add	%i0, 4, %i0		! increment source address
	srl	%i3, %l0, %i5		! upper src bits into lower dst bits
	or	%i4, %i5, %i5		! merge
	mov	24, %l3			! align destination
1:
	srl	%i5, %l3, %i4		! prepare to write a single byte
	stb	%i4, [%i1]		! write a byte
	add	%i1, 1, %i1		! increment destination address
	sub	%i2, 1, %i2		! decrement count
	btst	3, %i1			! is destination aligned?
	bnz,a	1b
	  sub	%l3, 8, %l3		! delay slot, decrement shift count (LD)
	sub	%l5, %l3, %l2		! generate shift left count (UD)
	sll	%i5, %l2, %i5		! move leftover into upper bytes
	cmp	%l2, %l0		! cmp # reqd to fill dst w old src left
	bgu	%xcc, more_needed	! need more to fill than we have
	  nop

	sll	%i3, %l1, %i3		! clear upper used byte(s)
	srl	%i3, %l1, %i3
	! get the odd bytes between alignments
	sub	%l0, %l2, %l0		! regenerate shift count
	sub	%l5, %l0, %l1		! generate new shift left count (LS)
	and	%i2, 3, %l4		! must do remaining bytes if count%4 > 0
	andn	%i2, 3, %i2		! # of aligned bytes that can be moved
	srl	%i3, %l0, %i4
	or	%i5, %i4, %i5
	st	%i5, [%i1]		! write a word
	subcc	%i2, 4, %i2		! decrement count
	bz	%xcc, unalign_out
	  add	%i1, 4, %i1		! increment destination address

	b	2f
	sll	%i3, %l1, %i5		! get leftover into upper bits
more_needed:
	sll	%i3, %l0, %i3		! save remaining byte(s)
	srl	%i3, %l0, %i3
	sub	%l2, %l0, %l1		! regenerate shift count
	sub	%l5, %l1, %l0		! generate new shift left count
	sll	%i3, %l1, %i4		! move to fill empty space
	b	3f
	or	%i5, %i4, %i5		! merge to complete word
	!
	! the source address is aligned and destination is not
	!
align_dst_only:
	ld	[%i0], %i4		! read a word
	add	%i0, 4, %i0		! increment source address
	mov	24, %l0			! initial shift alignment count
1:
	srl	%i4, %l0, %i3		! prepare to write a single byte
	stb	%i3, [%i1]		! write a byte
	add	%i1, 1, %i1		! increment destination address
	sub	%i2, 1, %i2		! decrement count
	btst	3, %i1			! is destination aligned?
	bnz,a	1b
	  sub	%l0, 8, %l0		! delay slot, decrement shift count
xfer:
	sub	%l5, %l0, %l1		! generate shift left count
	sll	%i4, %l1, %i5		! get leftover
3:
	and	%i2, 3, %l4		! must do remaining bytes if count%4 > 0
	andn	%i2, 3, %i2		! # of aligned bytes that can be moved
2:
	ld	[%i0], %i3		! read a source word
	add	%i0, 4, %i0		! increment source address
	srl	%i3, %l0, %i4		! upper src bits into lower dst bits
	or	%i5, %i4, %i5		! merge with upper dest bits (leftover)
	st	%i5, [%i1]		! write a destination word
	subcc	%i2, 4, %i2		! decrement count
	bz	%xcc, unalign_out	! check if done
	  add	%i1, 4, %i1		! increment destination address
	b	2b			! loop
	sll	%i3, %l1, %i5		! get leftover
unalign_out:
	tst	%l4			! any bytes leftover?
	bz	%xcc, cpdone
	  nop
1:
	sub	%l0, 8, %l0		! decrement shift
	srl	%i3, %l0, %i4		! upper src byte into lower dst byte
	stb	%i4, [%i1]		! write a byte
	subcc	%l4, 1, %l4		! decrement count
	bz	%xcc, cpdone		! done?
	add	%i1, 1, %i1		! increment destination
	tst	%l0			! any more previously read bytes
	bnz	%xcc, 1b		! we have leftover bytes
	  mov	%l4, %i2		! delay slot, mv cnt where dbytecp wants
	b	dbytecp			! let dbytecp do the rest
	  sub	%i0, %i1, %i0		! i0 gets the difference of src and dst
	!
	! the destination address is aligned and the source is not
	!
align_src_only:
	ldub	[%i0], %i3		! read a byte from source address
	add	%i0, 1, %i0		! increment source address
	or	%i4, %i3, %i4		! or in with previous bytes (if any)
	btst	3, %i0			! is source aligned?
	add	%l0, 8, %l0		! increment shift count (US)
	bnz,a	align_src_only
	  sll	%i4, 8, %i4		! make room for next byte
	b,a	xfer
	!
	! if from address unaligned for double-word moves,
	! move bytes till it is, if count is < 56 it could take
	! longer to align the thing than to do the transfer
	! in word size chunks right away
	!
aldoubcp:
	cmp	%i2, 56			! if count < 56, use wordcp, it takes
	blu,a	%xcc, alwordcp		! longer to align doubles than words
	  mov	3, %o0			! mask for word alignment
	call	alignit			! copy bytes until aligned
	mov	7, %o0			! mask for double alignment
	  !
	! source and destination are now double-word aligned
	! i3 has aligned count returned by alignit
	!
	and	%i2, 7, %i2		! unaligned leftover count
	sub	%i0, %i1, %i0		! i0 gets the difference of src and dst
5:
	ldx	[%i0+%i1], %o4		! read from address
	stx	%o4, [%i1]		! write at destination address
	subcc	%i3, 8, %i3		! dec count
	bgu	%xcc, 5b
	  add	%i1, 8, %i1		! delay slot, inc to address
	cmp	%i2, 4			! see if we can copy a word
	blu	%xcc, dbytecp		! if 3 or less bytes use bytecp
	  nop
	!
	! for leftover bytes we fall into wordcp, if needed
	!
wordcp:
	and	%i2, 3, %i2		! unaligned leftover count
5:
	ld	[%i0+%i1], %o4		! read from address
	st	%o4, [%i1]		! write at destination address
	subcc	%i3, 4, %i3		! dec count
	bgu	%xcc, 5b
	  add	%i1, 4, %i1		! delay slot, inc to address
	b,a	dbytecp

	! we come here to align copies on word boundaries
alwordcp:
	call	alignit		! go word-align it
	  mov	3, %o0			! bits that must be zero to be aligned
	b	wordcp
	  sub	%i0, %i1, %i0		! i0 gets the difference of src and dst

	!
	! byte copy, works with any alignment
	!
bytecp:
	b	dbytecp
	  sub	%i0, %i1, %i0		! i0 gets difference of src and dst

	!
	! differenced byte copy, works with any alignment
	! assumes dest in %i1 and (source - dest) in %i0
	!
1:
	stb	%o4, [%i1]		! write to address
	inc	%i1			! inc to address
dbytecp:
	deccc	%i2			! dec count
	bgeu,a	%xcc, 1b		! loop till done
	ldub	[%i0+%i1], %o4		! read from address
cpdone:
	membar	#Sync				! sync error barrier
	ret
	  restore %g0, 0, %o0		! return (0)

/*
 * Common code used to align transfers on word and doubleword
 * boudaries.  Aligns source and destination and returns a count
 * of aligned bytes to transfer in %i3
 */
1:
	inc	%i0			! inc from
	stb	%o4, [%i1]		! write a byte
	inc	%i1			! inc to
	dec	%i2			! dec count
alignit:
	btst	%o0, %i0		! %o0 is bit mask to check for alignment
	bnz,a	1b
	  ldub	[%i0], %o4		! read next byte

	retl
	andn	%i2, %o0, %i3		! return size of aligned bytes
END(novbcopy)


/*
 * hwblkclr - clears block-aligned, block-multiple-sized regions that are
 * longer than 256 bytes in length using Niagara's block stores/quad store.
 * If the criteria for using this routine are not met then it calls bzero
 * and returns 1.  Otherwise 0 is returned indicating success.
 * Caller is responsible for ensuring use_hw_bzero is true and that
 * kpreempt_disable() has been called.
 */
#ifdef lint
/*ARGSUSED*/
int
hwblkclr(void *addr, size_t len)
{ 
	return(0);
}
#else /* lint */
	! %i0 - start address
	! %i1 - length of region (multiple of 64)

ENTRY(hwblkclr)
	save	%sp, -SA(MINFRAME), %sp

	! Must be block-aligned
	andcc	%i0, 0x3f, %g0
	bnz,pn	%xcc, 1f
	  nop

	! ... and must be 256 bytes or more
	cmp	%i1, 0x100
	blu,pn	%xcc, 1f
	  nop

	! ... and length must be a multiple of 64
	andcc	%i1, 0x3f, %g0
	bz,pn	%xcc, pz_doblock
	wr	%g0, ASI_LDSTBI_P, %asi

1:	! punt, call bzero but notify the caller that bzero was used
	mov	%i0, %o0
	call	bzero
	  mov	%i1, %o1
	ret
	  restore	%g0, 1, %o0	! return (1) - did not use block operations

	! Already verified that there are at least 256 bytes to set
pz_doblock:
	stxa	%g0, [%i0+0x0]%asi
	stxa	%g0, [%i0+0x40]%asi
	stxa	%g0, [%i0+0x80]%asi
	stxa	%g0, [%i0+0xc0]%asi

	stxa	%g0, [%i0+0x8]%asi
	stxa	%g0, [%i0+0x10]%asi
	stxa	%g0, [%i0+0x18]%asi
	stxa	%g0, [%i0+0x20]%asi
	stxa	%g0, [%i0+0x28]%asi
	stxa	%g0, [%i0+0x30]%asi
	stxa	%g0, [%i0+0x38]%asi

	stxa	%g0, [%i0+0x48]%asi
	stxa	%g0, [%i0+0x50]%asi
	stxa	%g0, [%i0+0x58]%asi
	stxa	%g0, [%i0+0x60]%asi
	stxa	%g0, [%i0+0x68]%asi
	stxa	%g0, [%i0+0x70]%asi
	stxa	%g0, [%i0+0x78]%asi

	stxa	%g0, [%i0+0x88]%asi
	stxa	%g0, [%i0+0x90]%asi
	stxa	%g0, [%i0+0x98]%asi
	stxa	%g0, [%i0+0xa0]%asi
	stxa	%g0, [%i0+0xa8]%asi
	stxa	%g0, [%i0+0xb0]%asi
	stxa	%g0, [%i0+0xb8]%asi

	stxa	%g0, [%i0+0xc8]%asi
	stxa	%g0, [%i0+0xd0]%asi
	stxa	%g0, [%i0+0xd8]%asi
	stxa	%g0, [%i0+0xe0]%asi
	stxa	%g0, [%i0+0xe8]%asi
	stxa	%g0, [%i0+0xf0]%asi
	stxa	%g0, [%i0+0xf8]%asi

	sub	%i1, 0x100, %i1
	cmp	%i1, 0x100
	bgu,pt	%xcc, pz_doblock
	  add	%i0, 0x100, %i0

2:
	! Check if more than 64 bytes to set
	cmp	%i1,0x40
	blu	%xcc, pz_finish
	  nop

3:
	stxa	%g0, [%i0+0x0]%asi
	stxa	%g0, [%i0+0x8]%asi
	stxa	%g0, [%i0+0x10]%asi
	stxa	%g0, [%i0+0x18]%asi
	stxa	%g0, [%i0+0x20]%asi
	stxa	%g0, [%i0+0x28]%asi
	stxa	%g0, [%i0+0x30]%asi
	stxa	%g0, [%i0+0x38]%asi

	subcc	%i1, 0x40, %i1
	bgu,pt	%xcc, 3b
	  add	%i0, 0x40, %i0

pz_finish:
	membar	#Sync
	ret
	  restore	%g0, 0, %o0		! return (bzero or not)
END(hwblkclr)
#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
bzero(void *addr, size_t count)
{}

#else	/* lint */

ENTRY(bzero)
	wr	%g0, ASI_P, %asi

	cmp	%o1, 7
	blu,pn	%xcc, byteclr
	  nop

	cmp	%o1, 15
	blu,pn	%xcc, wdalign
	  nop

	andcc	%o0, 7, %o3		! is add aligned on a 8 byte bound
	bz,pt	%xcc, blkalign		! already double aligned
	  sub	%o3, 8, %o3		! -(bytes till double aligned)
	add	%o1, %o3, %o1		! update o1 with new count

1:
	stba	%g0, [%o0]%asi
	inccc	%o3
	bl,pt	%xcc, 1b
	  inc	%o0

	! Now address is double aligned
blkalign:
	cmp	%o1, 0x80		! check if there are 128 bytes to set
	blu,pn	%xcc, bzero_small
	  mov	%o1, %o3
#if 0
	sethi	%hi(use_hw_bzero), %o2
	ld	[%o2 + %lo(use_hw_bzero)], %o2
	tst	%o2
	bz	%xcc, bzero_small
	  mov	%o1, %o3
#endif
	rd	%asi, %o3
	wr	%g0, ASI_LDSTBI_P, %asi
	cmp	%o3, ASI_P
	bne,a	%xcc, algnblk
	  wr	%g0, ASI_LDSTBI_AIUS, %asi

algnblk:
	andcc	%o0, 0x3f, %o3		! is block aligned?
	bz,pt	%xcc, bzero_blk
	  sub	%o3, 0x40, %o3		! -(bytes till block aligned)
	add	%o1, %o3, %o1		! o1 is the remainder
	
	! Clear -(%o3) bytes till block aligned
1:
	stxa	%g0, [%o0]%asi
	addcc	%o3, 8, %o3
	bl,pt	%xcc, 1b
 	  add	%o0, 8, %o0

bzero_blk:
	and	%o1, 0x3f, %o3		! calc bytes left after blk clear
	andn	%o1, 0x3f, %o4		! calc size of blocks in bytes

	cmp	%o4, 0x100		! 256 bytes or more
	blu,pn	%xcc, 3f
	  nop

2:
	stxa	%g0, [%o0+0x0]%asi
	stxa	%g0, [%o0+0x40]%asi
	stxa	%g0, [%o0+0x80]%asi
	stxa	%g0, [%o0+0xc0]%asi

	stxa	%g0, [%o0+0x8]%asi
	stxa	%g0, [%o0+0x10]%asi
	stxa	%g0, [%o0+0x18]%asi
	stxa	%g0, [%o0+0x20]%asi
	stxa	%g0, [%o0+0x28]%asi
	stxa	%g0, [%o0+0x30]%asi
	stxa	%g0, [%o0+0x38]%asi

	stxa	%g0, [%o0+0x48]%asi
	stxa	%g0, [%o0+0x50]%asi
	stxa	%g0, [%o0+0x58]%asi
	stxa	%g0, [%o0+0x60]%asi
	stxa	%g0, [%o0+0x68]%asi
	stxa	%g0, [%o0+0x70]%asi
	stxa	%g0, [%o0+0x78]%asi

	stxa	%g0, [%o0+0x88]%asi
	stxa	%g0, [%o0+0x90]%asi
	stxa	%g0, [%o0+0x98]%asi
	stxa	%g0, [%o0+0xa0]%asi
	stxa	%g0, [%o0+0xa8]%asi
	stxa	%g0, [%o0+0xb0]%asi
	stxa	%g0, [%o0+0xb8]%asi

	stxa	%g0, [%o0+0xc8]%asi
	stxa	%g0, [%o0+0xd0]%asi
	stxa	%g0, [%o0+0xd8]%asi
	stxa	%g0, [%o0+0xe0]%asi
	stxa	%g0, [%o0+0xe8]%asi
	stxa	%g0, [%o0+0xf0]%asi
	stxa	%g0, [%o0+0xf8]%asi

	sub	%o4, 0x100, %o4
	cmp	%o4, 0x100
	bgu,pt	%xcc, 2b
	  add	%o0, 0x100, %o0

3:
	! ... check if 64 bytes to set
	cmp	%o4, 0x40
	blu	%xcc, bzero_blk_done
	  nop

4:
	stxa	%g0, [%o0+0x0]%asi
	stxa	%g0, [%o0+0x8]%asi
	stxa	%g0, [%o0+0x10]%asi
	stxa	%g0, [%o0+0x18]%asi
	stxa	%g0, [%o0+0x20]%asi
	stxa	%g0, [%o0+0x28]%asi
	stxa	%g0, [%o0+0x30]%asi
	stxa	%g0, [%o0+0x38]%asi

	subcc	%o4, 0x40, %o4
	bgu,pt	%xcc, 3b
	  add	%o0, 0x40, %o0

bzero_blk_done:
	membar	#Sync
	!
	! Undo asi register setting.
	!
	rd	%asi, %o4
	wr	%g0, ASI_P, %asi
	cmp	%o4, ASI_LDSTBI_P
	bne,a	%xcc, bzero_small
	  wr	%g0, ASI_AIUS, %asi

bzero_small:
	! Set the remaining doubles
	subcc	%o3, 8, %o3		! Can we store any doubles?
	blu,pn	%xcc, byteclr
	  and	%o1, 7, %o1		! calc bytes left after doubles

dbclr:
	stxa	%g0, [%o0]%asi		! Clear the doubles
	subcc	%o3, 8, %o3
	bgeu,pt	%xcc, dbclr
	  add	%o0, 8, %o0

	ba	byteclr
 	  nop

wdalign:			
	andcc	%o0, 3, %o3		! is add aligned on a word boundary
	bz,pn	%xcc, wdclr
	andn	%o1, 3, %o3		! create word sized count in %o3

	dec	%o1			! decrement count
	stba	%g0, [%o0]%asi		! clear a byte
	ba	wdalign
	  inc	%o0			! next byte

wdclr:
	sta	%g0, [%o0]%asi		! 4-byte clearing loop
	subcc	%o3, 4, %o3
	bnz,pt	%xcc, wdclr
	  inc	4, %o0

	and	%o1, 3, %o1		! leftover count, if any

byteclr:
	! Set the leftover bytes
	brz	%o1, bzero_exit
	nop

7:
	deccc	%o1			! byte clearing loop
	stba	%g0, [%o0]%asi
	bgu,pt	%xcc, 7b
	  inc	%o0

bzero_exit:
	retl
	  clr	%o0			! return (0)

END(bzero)
#endif	/* lint */


#if 0
#define SMALL_LIMIT 7	
#if defined(lint)

/*ARGSUSED*/
int
copyin(const void *uaddr, void *kaddr, size_t count)
{ return (0); }

#else	/* lint */

ENTRY(copyin)
	!
	! Check the length and bail if zero.
	!
	tst	%o2
	bnz,pt	%xcc, 1f
	  nop
	retl
	  clr	%o0
#if 0
1:
	sethi	%hi(copyio_fault), %o4
	or	%o4, %lo(copyio_fault), %o4
	sethi	%hi(copyio_fault_nowindow), %o3
	ldn	[THREAD_REG + T_LOFAULT], SAVED_LOFAULT
	or	%o3, %lo(copyio_fault_nowindow), %o3
	membar	#Sync
	stn	%o3, [THREAD_REG + T_LOFAULT]

	mov	%o0, SAVE_SRC
	mov	%o1, SAVE_DST
	mov	%o2, SAVE_COUNT
#endif
	!
	! Check to see if we're more than SMALL_LIMIT.
	!
	subcc	%o2, SMALL_LIMIT, %o3
	bgu,a,pt %xcc, dci_ns
	  or	%o0, %o1, %o3
	!
	! What was previously ".small_copyin"
	!
dcibcp:
	sub	%g0, %o2, %o3		! setup for copy loop
	add	%o0, %o2, %o0
	add	%o1, %o2, %o1
	ba,pt	%xcc, dcicl
	lduba	[%o0 + %o3]ASI_AIUS, %o4
	!
	! %o0 and %o1 point at the end and remain pointing at the end
	! of their buffers. We pull things out by adding %o3 (which is
	! the negation of the length) to the buffer end which gives us
	! the curent location in the buffers. By incrementing %o3 we walk
	! through both buffers without having to bump each buffer's
	! pointer. A very fast 4 instruction loop.
	!
	.align 16
dcicl:
	stb	%o4, [%o1 + %o3]
	inccc	%o3
	bl,a,pt %xcc, dcicl
	lduba	[%o0 + %o3]ASI_AIUS, %o4
	!
	! We're done. Go home.
	!	
	membar	#Sync
	retl
	  clr	%o0
	!
	! Try aligned copies from here.
	!
dci_ns:
	!
	! See if we're single byte aligned. If we are, check the
	! limit for single byte copies. If we're smaller, or equal,
	! bounce to the byte for byte copy loop. Otherwise do it in
	! HW (if enabled).
	!
	btst	1, %o3
	bz,a,pt	%icc, dcih8
	btst	7, %o3
	!
	! We're single byte aligned.
	!
	sethi	%hi(hw_copy_limit_1), %o3
	ld	[%o3 + %lo(hw_copy_limit_1)], %o3
	!
	! Is HW copy on? If not do everything byte for byte.
	!
	tst	%o3
	bz,pn	%icc, dcibcp
	  subcc	%o3, %o2, %o3
	!
	! Are we bigger than the HW limit? If not
	! go to byte for byte.
	!
	bge,pt	%xcc, dcibcp
	  nop
	!
	! We're big enough and copy is on. Do it with HW.
	!
	ba,pt	%xcc, big_copyin
	nop
dcih8:
	!
	! 8 byte aligned?
	!
	bnz,a	%xcc, dcih4
	btst	3, %o3
	!
	! We're eight byte aligned.
	!
	sethi	%hi(hw_copy_limit_8), %o3
	ld	[%o3 + %lo(hw_copy_limit_8)], %o3
	!
	! Is HW assist on? If not, do it with the aligned copy.
	!
	tst	%o3
	bz,pn	%icc, dcis8
	subcc	%o3, %o2, %o3
	bge	%xcc, dcis8
	nop
	ba,pt	%xcc, big_copyin
	nop
dcis8:
	!
	! Housekeeping for copy loops. Uses same idea as in the byte for
	! byte copy loop above.
	!
	add	%o0, %o2, %o0
	add	%o1, %o2, %o1
	sub	%g0, %o2, %o3
	ba,pt	%xcc, didebc
	srl	%o2, 3, %o2		! Number of 8 byte chunks to copy
	!
	! 4 byte aligned?
	!
dcih4:
	bnz	%xcc, dcih2
	sethi	%hi(hw_copy_limit_4), %o3
	ld	[%o3 + %lo(hw_copy_limit_4)], %o3
	!
	! Is HW assist on? If not, do it with the aligned copy.
	!
	tst	%o3
	bz,pn	%icc, dcis4
	  subcc	%o3, %o2, %o3
	!
	! We're negative if our size is less than or equal to hw_copy_limit_4.
	!
	bge	%xcc, dcis4
	  nop
	ba,pt	%xcc, big_copyin
	  nop
dcis4:
	!
	! Housekeeping for copy loops. Uses same idea as in the byte
	! for byte copy loop above.
	!
	add	%o0, %o2, %o0
	add	%o1, %o2, %o1
	sub	%g0, %o2, %o3
	ba,pt	%xcc, didfbc
	  srl	%o2, 2, %o2		! Number of 4 byte chunks to copy
dcih2:
	!
	! We're two byte aligned. Check for "smallness"
	! done in delay at .dcih4
	!
	bleu,pt	%xcc, dcis2
	sethi	%hi(hw_copy_limit_2), %o3
	ld	[%o3 + %lo(hw_copy_limit_2)], %o3
	!
	! Is HW assist on? If not, do it with the aligned copy.
	!
	tst	%o3
	bz,pn	%icc, dcis2
	  subcc	%o3, %o2, %o3
	!
	! Are we larger than the HW limit?
	!
	bge	%xcc, dcis2
	nop
	!
	! HW assist is on and we're large enough to use it.
	!
	ba,pt	%xcc, big_copyin
	nop
	!
	! Housekeeping for copy loops. Uses same idea as in the byte
	! for byte copy loop above.
	!
dcis2:
	add	%o0, %o2, %o0
	add	%o1, %o2, %o1
	sub	%g0, %o2, %o3
	ba,pt	%xcc, didtbc
	srl	%o2, 1, %o2		! Number of 2 byte chunks to copy
	!
small_copyin:
	!
	! Why are we doing this AGAIN? There are certain conditions in
	! big copyin that will cause us to forgo the HW assisted copys
	! and bounce back to a non-hw assisted copy. This dispatches
	! those copies. Note that we branch around this in the main line
	! code.
	!
	! We make no check for limits or HW enablement here. We've
	! already been told that we're a poster child so just go off
	! and do it.
	!
	or	%o0, %o1, %o3
	btst	1, %o3
	bnz	%icc, dcibcp		! Most likely
	  btst	7, %o3
	bz	%icc, dcis8
	  btst	3, %o3
	bz	%icc, dcis4
	  nop
	ba,pt	%xcc, dcis2
	  nop
	!
	! Eight byte aligned copies. A steal from the original .small_copyin
	! with modifications. %o2 is number of 8 byte chunks to copy. When
	! done, we examine %o3. If this is < 0, we have 1 - 7 bytes more
	! to copy.
	!
	.align 32
didebc:
	ldxa	[%o0 + %o3]ASI_AIUS, %o4
	deccc	%o2
	stx	%o4, [%o1 + %o3]
	bg,pt	%xcc, didebc
	addcc	%o3, 8, %o3
	!
	! End of copy loop. Most 8 byte aligned copies end here.
	!
	bz,pt	%xcc, dcifh
	nop
	!
	! Something is left. Do it byte for byte.
	!
	ba,pt	%xcc, dcicl
	lduba	[%o0 + %o3]ASI_AIUS, %o4
	!
	! 4 byte copy loop. %o2 is number of 4 byte chunks to copy.
	!
	.align 32
didfbc:
	lduwa	[%o0 + %o3]ASI_AIUS, %o4
	deccc	%o2
	st	%o4, [%o1 + %o3]
	bg,pt	%xcc, didfbc
	addcc	%o3, 4, %o3
	!
	! End of copy loop. Most 4 byte aligned copies end here.
	!
	bz,pt	%xcc, dcifh
	nop
	!
	! Something is left. Do it byte for byte.
	!
	ba,pt	%xcc, dcicl
	lduba	[%o0 + %o3]ASI_AIUS, %o4
	!
	! 2 byte aligned copy loop. %o2 is number of 2 byte chunks to
	! copy.
	!
	.align 32
didtbc:
	lduha	[%o0 + %o3]ASI_AIUS, %o4
	deccc	%o2
	sth	%o4, [%o1 + %o3]
	bg,pt	%xcc, didtbc
	  addcc	%o3, 2, %o3
	!
	! End of copy loop. Most 2 byte aligned copies end here.
	!
	bz,pt	%xcc, dcifh
	  nop
	!
	! Deal with the last byte
	!
	lduba	[%o0 + %o3]ASI_AIUS, %o4
	stb	%o4, [%o1 + %o3]
dcifh:
	membar	#Sync
	retl
	  clr	%o0

big_copyin:
	!
	! We're going off to do a block copy.
	! Switch fault hendlers and grab a window. We
	! don't do a membar #Sync since we've done only
	! kernel data to this point.
	!
	save	%sp, -SA(MINFRAME), %sp

	! Copy in that reach here are larger than 256 bytes. The
	! hw_copy_limit_1 is set to 256. Never set this limit less
	! 128 bytes.
do_blockcopyin:

	! Swap src/dst since the code below is memcpy code
	! and memcpy/bcopy have different calling sequences
	mov	%i1, %i5
	mov	%i0, %i1
	mov	%i5, %i0

	andcc	%i0, 7, %i3		! is dst double aligned
	bz	%xcc, copyin_blkcpy
	  sub	%i3, 8, %i3
	neg	%i3			! bytes till double aligned
	sub	%i2, %i3, %i2		! update %i2 with new count

	! Align Destination on double-word boundary

1:	lduba	[%i1]ASI_AIUS, %i4
	inc	%i1
	stb	%i4, [%i0]
	deccc	%i3
	bgu	%xcc, 1b
	  inc	%i0

copyin_blkcpy:
	andcc	%i0, 63, %i3
	bz,pn	%xcc, copyin_blalign	! now block aligned
	sub	%i3, 64, %i3
	neg	%i3			! bytes till block aligned
	sub	%i2, %i3, %i2		! update %i2 with new count

	! Copy %i3 bytes till dst is block (64 byte) aligned. use
	! double word copies.

	andcc	%i1, 7, %g1		! is src aligned on a 8 bytes
	bz	%xcc, ci_dbcopy		! %g1 has source offset (last 3-bits)
	sll	%g1, 3, %l1		! left shift
	mov	0x40, %l2
	sub	%l2, %l1, %l2		! right shift = (64 - left shift)

	! Now use double word copies to align destination.
ci_double:
	sub	%i1, %g1, %i1		! align the src at 8 bytes.
	ldxa	[%i1]ASI_AIUS, %o2
2:
	add	%i1, 0x8, %i1
	ldxa	[%i1]ASI_AIUS, %o4
	ALIGN_DATA_EW(%o2, %o4, %l1, %l2, %o3)
	stx	%o2, [%i0]
	mov	%o4, %o2
	subcc	%i3, 0x8, %i3
	bgu,pt	%xcc, 2b
	add	%i0, 0x8, %i0
	ba	copyin_blalign
	add	%i1, %g1, %i1

	! Both source and destination are double aligned.
	! No shift and merge of data required in this case.
ci_dbcopy:
	ldxa	[%i1]ASI_AIUS, %o2
	stx	%o2, [%i0]
	add	%i1, 0x8, %i1
	subcc	%i3, 0x8, %i3
	bgu,pt	%xcc, ci_dbcopy
	add	%i0, 0x8, %i0

copyin_blalign:
	andn	%i2, 0x3f, %i3		! %i3 count is multiple of block size
	sub	%i2, %i3, %i2		! Residue bytes in %i2

	wr	%g0, ASI_LDSTBI_P, %asi

	andcc	%i1, 0xf, %o2		! is src quadword aligned
	bz,pn	%xcc, ci_blkcpy		! src offset in %o2 (last 4-bits)
	  nop
	cmp	%o2, 0x8
	bg	ci_upper_double
	  nop
	bl	ci_lower_double
	  nop

	! Falls through when source offset is equal to 8 i.e.
	! source is double word aligned.
	! In this case no shift/merge of data is required

	sub	%i1, %o2, %i1		! align the src at 16 bytes.
	andn	%i1, 0x3f, %l0		! %l0 has block aligned source
	prefetch [%l0+0x0], #one_read
	ldda	[%i1]ASI_LDSTBI_AIUS, %l2
ci_loop0:
	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l4

	prefetch [%l0+0x40], #one_read

	stxa	%l3, [%i0+0x0]%asi
	stxa	%l4, [%i0+0x8]%asi

	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l2

	stxa	%l5, [%i0+0x10]%asi
	stxa	%l2, [%i0+0x18]%asi

	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l4

	stxa	%l3, [%i0+0x20]%asi
	stxa	%l4, [%i0+0x28]%asi

	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l2

	stxa	%l5, [%i0+0x30]%asi
	stxa	%l2, [%i0+0x38]%asi

	add	%l0, 0x40, %l0
	subcc	%i3, 0x40, %i3
	bgu,pt	%xcc, ci_loop0
	  add	%i0, 0x40, %i0
	ba	ci_blkdone
	  add	%i1, %o2, %i1		! increment the source by src offset
					! the src offset was stored in %o2

ci_lower_double:

	sub	%i1, %o2, %i1		! align the src at 16 bytes.
	sll	%o2, 3, %o0		! %o0 left shift
	mov	0x40, %o1
	sub	%o1, %o0, %o1		! %o1 right shift = (64 - left shift)
	andn	%i1, 0x3f, %l0		! %l0 has block aligned source
	prefetch [%l0+0x0], #one_read
	ldda	[%i1]ASI_LDSTBI_AIUS, %l2	! partial data in %l2
							! and %l3 has complete
							! data
ci_loop1:
	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l4	! %l4 has partial data
							! for this read.
	ALIGN_DATA(%l2, %l3, %l4, %o0, %o1, %l6)	! merge %l2, %l3 and %l4
							! into %l2 and %l3

	prefetch [%l0+0x40], #one_read

	stxa	%l2, [%i0+0x0]%asi
	stxa	%l3, [%i0+0x8]%asi

	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l2
	ALIGN_DATA(%l4, %l5, %l2, %o0, %o1, %l6)	! merge %l2 with %l5 and
							! %l4 from previous read
							! into %l4 and %l5
	stxa	%l4, [%i0+0x10]%asi
	stxa	%l5, [%i0+0x18]%asi

	! Repeat the same for next 32 bytes.

	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l4
	ALIGN_DATA(%l2, %l3, %l4, %o0, %o1, %l6)

	stxa	%l2, [%i0+0x20]%asi
	stxa	%l3, [%i0+0x28]%asi

	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l2
	ALIGN_DATA(%l4, %l5, %l2, %o0, %o1, %l6)

	stxa	%l4, [%i0+0x30]%asi
	stxa	%l5, [%i0+0x38]%asi

	add	%l0, 0x40, %l0
	subcc	%i3, 0x40, %i3
	bgu,pt	%xcc, ci_loop1
	  add	%i0, 0x40, %i0
	ba	ci_blkdone
	  add	%i1, %o2, %i1		! increment the source by src offset
					! the src offset was stored in %o2

ci_upper_double:

	sub	%i1, %o2, %i1		! align the src at 16 bytes.
	sub	%o2, 0x8, %o0
	sll	%o0, 3, %o0		! %o0 left shift
	mov	0x40, %o1
	sub	%o1, %o0, %o1		! %o1 right shift = (64 - left shift)
	andn	%i1, 0x3f, %l0		! %l0 has block aligned source
	prefetch [%l0+0x0], #one_read
	ldda	[%i1]ASI_LDSTBI_AIUS, %l2	! partial data in %l3
							! for this read and
							! no data in %l2
ci_loop2:
	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l4	! %l4 has complete data
							! and %l5 has partial
	ALIGN_DATA(%l3, %l4, %l5, %o0, %o1, %l6)	! merge %l3, %l4 and %l5
							! into %l3 and %l4
	prefetch [%l0+0x40], #one_read

	stxa	%l3, [%i0+0x0]%asi
	stxa	%l4, [%i0+0x8]%asi

	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l2
	ALIGN_DATA(%l5, %l2, %l3, %o0, %o1, %l6)	! merge %l2 and %l3 with
							! %l5 from previous read
							! into %l5 and %l2

	stxa	%l5, [%i0+0x10]%asi
	stxa	%l2, [%i0+0x18]%asi

	! Repeat the same for next 32 bytes.

	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l4
	ALIGN_DATA(%l3, %l4, %l5, %o0, %o1, %l6)

	stxa	%l3, [%i0+0x20]%asi
	stxa	%l4, [%i0+0x28]%asi

	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l2
	ALIGN_DATA(%l5, %l2, %l3, %o0, %o1, %l6)

	stxa	%l5, [%i0+0x30]%asi
	stxa	%l2, [%i0+0x38]%asi

	add	%l0, 0x40, %l0
	subcc	%i3, 0x40, %i3
	bgu,pt	%xcc, ci_loop2
	  add	%i0, 0x40, %i0
	ba	ci_blkdone
	  add	%i1, %o2, %i1		! increment the source by src offset
					! the src offset was stored in %o2


	! Do fast copy using ASI_LDSTBI_P
ci_blkcpy:

	andn	%i1, 0x3f, %o0		! %o0 has block aligned source
	prefetch [%o0+0x0], #one_read
1:
	ldda	[%i1]ASI_LDSTBI_AIUS, %l0
	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l2
	add	%i1, 0x10, %i1

	prefetch [%o0+0x40], #one_read

	stxa	%l0, [%i0+0x0]%asi

	ldda	[%i1]ASI_LDSTBI_AIUS, %l4
	add	%i1, 0x10, %i1
	ldda	[%i1]ASI_LDSTBI_AIUS, %l6
	add	%i1, 0x10, %i1

	stxa	%l1, [%i0+0x8]%asi
	stxa	%l2, [%i0+0x10]%asi
	stxa	%l3, [%i0+0x18]%asi
	stxa	%l4, [%i0+0x20]%asi
	stxa	%l5, [%i0+0x28]%asi
	stxa	%l6, [%i0+0x30]%asi
	stxa	%l7, [%i0+0x38]%asi

	add	%o0, 0x40, %o0
	subcc	%i3, 0x40, %i3
	bgu,pt	%xcc, 1b
	  add	%i0, 0x40, %i0

ci_blkdone:
	membar	#Sync

	! Copy as much rest of the data as double word copy.
ci_dwcp:
	cmp	%i2, 0x8		! Not enough bytes to copy as double
	blu	%xcc, ci_dbdone
	  nop

	andn	%i2, 0x7, %i3		! %i3 count is multiple of 8 bytes size
	sub	%i2, %i3, %i2		! Residue bytes in %i2

	andcc	%i1, 7, %g1		! is src aligned on a 8 bytes
	bz	%xcc, ci_cpy_db
	  nop

	sll	%g1, 3, %l0		! left shift
	mov	0x40, %l1
	sub	%l1, %l0, %l1		! right shift = (64 - left shift)

ci_cpy_dbwd:
	sub	%i1, %g1, %i1		! align the src at 8 bytes.
	ldxa	[%i1]ASI_AIUS, %o2
3:
	add	%i1, 0x8, %i1
	ldxa	[%i1]ASI_AIUS, %o4
	ALIGN_DATA_EW(%o2, %o4, %l0, %l1, %o3)
	stx	%o2, [%i0]
	mov	%o4, %o2
	subcc	%i3, 0x8, %i3
	bgu,pt	%xcc, 3b
	  add	%i0, 0x8, %i0
	ba	ci_dbdone
	  add	%i1, %g1, %i1

ci_cpy_db:
	ldxa	[%i1]ASI_AIUS, %o2
	stx	%o2, [%i0]
	add	%i1, 0x8, %i1
	subcc	%i3, 0x8, %i3
	bgu,pt	%xcc, ci_cpy_db
	add	%i0, 0x8, %i0

ci_dbdone:
	tst	%i2
	bz,pt	%xcc, copyin_exit
	  nop

	! Copy the residue as byte copy
ci_residue:
	lduba	[%i1]ASI_AIUS, %i4
	stb	%i4, [%i0]
	inc	%i1
	deccc	%i2
	bgu	%xcc, ci_residue
	  inc	%i0

copyin_exit:
	membar	#Sync
	ret
	  restore	%g0, 0, %o0
END(copyin)

#endif	/* lint */
#endif

