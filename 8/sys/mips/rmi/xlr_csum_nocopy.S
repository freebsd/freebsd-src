#include <machine/asm.h>


/*
 * a0: source address
 * a1: length of the area to checksum
 * a2: partial checksum
 * a3: dst
 */

#define src a0
#define dst a3
#define sum v0

	.text
	.set	noreorder

	.macro CSUM_BIGCHUNK_AND_COPY offset 
	pref                    0,  (\offset+0x0)(a0)
	ld			t0, (\offset+0x00)(a0)
	ld			t1, (\offset+0x08)(a0)
	.word			0x70481038  /*daddwc v0, v0, t0 */
	.word			0x70491038 /*daddwc v0, v0, t1 */
	ld			t0, (\offset + 0x10)(a0)		
	ld			t1, (\offset + 0x18)(a0)	
	.word			0x70481038 /* daddwc v0, v0, t0 */
	.word			0x70491038 /*daddwc v0, v0, t1 */
	.endm

small_csumcpy: 						/* unknown src alignment and < 8 bytes to go  */
	move 		a1, t2

	andi		t0, a1, 4
	beqz		t0, 1f
	andi		t0, a1, 2

	ulw			t1, (src) 			/* Still a full word to go  */
	daddiu		src, 4
	.word			0x70491038 /*daddwc v0, v0, t1 */

1:	move		t1, zero
	beqz		t0, 1f
	andi		t0, a1, 1

	ulhu		t1, (src) 			/* Still a halfword to go  */
	daddiu		src, 2

1:	beqz		t0, 1f
	sll			t1, t1, 16

	lbu			t2, (src)
	nop

#ifdef __MIPSEB__
	sll		t2, t2, 8
#endif
	or		t1, t2

1: 	.word			0x70491038 /*daddwc v0, v0, t1 */

	.word			0x70461038 /*daddwc v0, v0, a2 */
	.word			0x70401038 /*daddwc v0, v0, $0 */

	/* Ideally at this point of time the status flag must be cleared */
					
	dsll32      v1, sum, 0
	.word			0x70431038 /*daddwc v0, v0, v1 */
	dsrl32		sum, sum, 0
	.word			0x70401038 /*daddwc v0, v0, zero */

	/* fold the checksum */
	sll             v1, sum, 16
	addu            sum, v1
	sltu            v1, sum, v1
	srl             sum, sum, 16
	addu            sum, v1
1:
	.set		reorder
	jr			ra
	.set		noreorder

/* ------------------------------------------------------------------ */

	.align	5
LEAF(xlr_csum_partial_nocopy)
	move		sum, zero
	move		t7, zero

	sltiu		t8, a1, 0x8
	bnez		t8, small_csumcpy		/* < 8 bytes to copy */
	move		t2, a1

	beqz		a1, out
	andi		t7, src, 0x1			/* odd buffer? */

hword_align:
	beqz		t7, word_align
	andi		t8, src, 0x2

	lbu			t0, (src)
	dsubu		a1, a1, 0x1
	.word			0x70481038 /*daddwc v0, v0, t0 */
	daddu		src, src, 0x1
	andi		t8, src, 0x2

word_align:
	beqz		t8, dword_align
	sltiu		t8, a1, 56

	lhu			t0, (src)
	dsubu		a1, a1, 0x2
	.word			0x70481038 /*daddwc v0, v0, t0 */
	sltiu		t8, a1, 56
	daddu		src, src, 0x2

dword_align:
	bnez		t8, do_end_words
	move		t8, a1

	andi		t8, src, 0x4
	beqz		t8, qword_align
	andi		t8, src, 0x8

	lw			t0, 0x00(src)
	dsubu		a1, a1, 0x4
	.word			0x70481038 /*daddwc v0, v0, t0 */
	daddu		src, src, 0x4
	andi		t8, src, 0x8

qword_align:
	beqz		t8, oword_align
	andi		t8, src, 0x10

	ld			t0, 0x00(src)
	dsubu		a1, a1, 0x8
	.word			0x70481038 /*daddwc v0, v0, t0 */
	daddu		src, src, 0x8
	andi		t8, src, 0x10

oword_align:
	beqz		t8, begin_movement
	dsrl		t8, a1, 0x7

	ld			t3, 0x08(src)
	ld			t0, 0x00(src)
	.word			0x704b1038 /*daddwc v0, v0, t3 */
	.word			0x70481038 /*daddwc v0, v0, t0 */
	dsubu		a1, a1, 0x10
	daddu		src, src, 0x10
	dsrl		t8, a1, 0x7

begin_movement:
	beqz		t8, 1f
	andi		t2, a1, 0x40

move_128bytes:
	pref		0, 0x20(a0)
	pref		0, 0x40(a0)
	pref		0, 0x60(a0)
	CSUM_BIGCHUNK_AND_COPY(0x00)
	CSUM_BIGCHUNK_AND_COPY(0x20)
	CSUM_BIGCHUNK_AND_COPY(0x40)
	CSUM_BIGCHUNK_AND_COPY(0x60)
	dsubu		t8, t8, 0x01
	bnez		t8, move_128bytes	/* flag */
	daddu		src, src, 0x80

1:
	beqz		t2, 1f
	andi		t2, a1, 0x20

move_64bytes:
	pref		0, 0x20(a0)
	pref		0, 0x40(a0)
	CSUM_BIGCHUNK_AND_COPY(0x00)
	CSUM_BIGCHUNK_AND_COPY(0x20)
	daddu	src, src, 0x40

1:
	beqz		t2, do_end_words
	andi		t8, a1, 0x1c

move_32bytes:
	pref		0, 0x20(a0)
	CSUM_BIGCHUNK_AND_COPY(0x00)
	andi		t8, a1, 0x1c
	daddu		src, src, 0x20

do_end_words:
	beqz		t8, maybe_end_cruft
	dsrl		t8, t8, 0x2

end_words:
	lw			t0, (src)
	dsubu		t8, t8, 0x1
	.word			0x70481038 /*daddwc v0, v0, t0 */
	bnez		t8, end_words
	daddu		src, src, 0x4

maybe_end_cruft:
	andi		t2, a1, 0x3

small_memcpy:
 j small_csumcpy; move a1, t2
	beqz		t2, out
	move		a1, t2

end_bytes:
	lb			t0, (src)
	dsubu		a1, a1, 0x1
	bnez		a2, end_bytes
	daddu		src, src, 0x1

out:
	jr			ra
	move		v0, sum
	END(xlr_csum_partial_nocopy)
