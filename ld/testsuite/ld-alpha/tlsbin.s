	.section ".tbss", "awT", @nobits
	.globl bg1, bg2, bg3, bg4, bg5, bg6, bg7, bg8
bg1:	.space 4
bg2:	.space 4
bg3:	.space 4
bg4:	.space 4
bg5:	.space 4
bg6:	.space 4
bg7:	.space 4
bg8:	.space 4
bl1:	.space 4
bl2:	.space 4
bl3:	.space 4
bl4:	.space 4
bl5:	.space 4
bl6:	.space 4
bl7:	.space 4
bl8:	.space 4

	.text
	.globl	_start
	.ent	_start
_start:
	rduniq
	mov	$0, $9

	/* IE */
	ldq	$1, sG2($gp)	!gottprel
	addq	$1, $9, $1

	/* IE against global symbol in exec */
	ldq	$1, bl1($gp)	!gottprel
	addq	$1, $9, $1

	/* LE */
	lda	$1, sg1($9)	!tprel
	lda	$1, bl2+2($9)	!tprel

	ldah	$1, sh2+3($9)	!tprelhi
	lda	$1, sh2+3($1)	!tprello

	ldq	$1, bl2+4($gp)	!gottprel
	addq	$1, $9, $1

	ret
	.end	_start
