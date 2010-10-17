	.section ".tbss"
	.align	4
	.globl bg1, bg2, bg3, bg4, bg5, bg6, bg7, bg8
bg1:	.word 0
bg2:	.word 0
bg3:	.word 0
bg4:	.word 0
bg5:	.word 0
bg6:	.word 0
bg7:	.word 0
bg8:	.word 0
bl1:	.word 0
bl2:	.word 0
bl3:	.word 0
bl4:	.word 0
bl5:	.word 0
bl6:	.word 0
bl7:	.word 0
bl8:	.word 0
	.text
	.globl	_start
	.type	_start,#function
	.proc	04
_start:
	save	%sp, -104, %sp
	.hidden	_GLOBAL_OFFSET_TABLE_
	sethi	%hi(_GLOBAL_OFFSET_TABLE_), %l4
	or	%l4, %lo(_GLOBAL_OFFSET_TABLE_), %l4
	nop;nop;nop;nop

	/* IE against global var  */
	sethi	%tie_hi22(sG6), %o3
	add	%o3, %tie_lo10(sG6), %o3
	ld	[%l4 + %o3], %o2, %tie_ld(sG6)
	add	%g7, %o2, %o4, %tie_add(sG6)
	nop;nop;nop;nop

	/* IE -> LE against global var defined in exec  */
	sethi	%tie_hi22(bg6), %o3
	add	%o3, %tie_lo10(bg6), %o5
	ld	[%l4 + %o5], %o2, %tie_ld(bg6)
	add	%g7, %o2, %o4, %tie_add(bg6)
	nop;nop;nop;nop

	/* IE -> LE against local var  */
	sethi	%tie_hi22(bl6), %o3
	add	%o3, %tie_lo10(bl6), %o5
	ld	[%l4 + %o5], %l2, %tie_ld(bl6)
	add	%g7, %l2, %l2, %tie_add(bl6)
	nop;nop;nop;nop

	/* direct %g7 access IE -> LE against local var  */
	sethi	%tie_hi22(bl8), %o3
	add	%o3, %tie_lo10(bl8), %o5
	ld	[%l4 + %o5], %l2, %tie_ld(bl8)
	ld	[%g7 + %l2], %l2, %tie_add(bl8)
	nop;nop;nop;nop

	/* IE -> LE against hidden but not local var  */
	sethi	%tie_hi22(sh6), %o3
	add	%o3, %tie_lo10(sh6), %o5
	ld	[%l4 + %o5], %l2, %tie_ld(sh6)
	add	%g7, %l2, %l2, %tie_add(sh6)
	nop;nop;nop;nop

	/* direct %g7 access IE -> LE against hidden but not local var  */
	sethi	%tie_hi22(bl8), %o3
	add	%o3, %tie_lo10(bl8), %o5
	ld	[%l4 + %o5], %l2, %tie_ld(bl8)
	stb	%l1, [%g7 + %l2], %tie_add(bl8)
	nop;nop;nop;nop

	/* LE, global var defined in exec  */
	sethi	%tle_hix22(sg2), %l1
	nop
	xor	%l1, %tle_lox10(sg2), %l2
	nop
	add	%g7, %l2, %l3
	nop;nop;nop;nop

	/* LE, local var  */
	sethi	%tle_hix22(bl2+2), %o0
	xor	%o0, %tle_lox10(bl2+2), %o0
	add	%g7, %o0, %o0
	nop;nop;nop;nop

	/* LE, hidden var defined in exec */
	sethi	%tle_hix22(sh2+1), %l1
	xor	%l1, %tle_lox10(sh2+1), %o5
	add	%g7, %o5, %o1
	nop;nop;nop;nop

	/* Direct %g7 access  */

	/* LE, global var defined in exec  */
	sethi	%tle_hix22(sg3), %l1
	xor	%l1, %tle_lox10(sg3), %o5
	ld	[%g7 + %o5], %o1
	nop;nop;nop;nop

	/* LE, local var  */
	sethi	%tle_hix22(bl3 + 3), %o0
	xor	%o0, %tle_lox10(bl3 + 3), %o0
	stb	%o1, [%g7 + %o0]
	nop;nop;nop;nop

	/* LE, hidden var defined in exec  */
	sethi	%tle_hix22(sh3 + 3), %o2
	xor	%o2, %tle_lox10(sh3 + 3), %o4
	ldstub	[%g7 + %o4], %o5
	nop;nop;nop;nop

	ret
	restore
