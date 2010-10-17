	.data
	.align	4096
	.section ".tbss"
	.align	4
bl1:	.word 0
bl2:	.word 0
bl3:	.word 0
bl4:	.word 0
bl5:	.word 0
	.text
	.align	4096
	.globl	fn3
	.type	fn3,#function
	.proc	04
fn3:
	save	%sp, -104, %sp
	.hidden	_GLOBAL_OFFSET_TABLE_
	sethi	%hi(_GLOBAL_OFFSET_TABLE_), %l1
	or	%l1, %lo(_GLOBAL_OFFSET_TABLE_), %l1
	nop;nop;nop;nop

	/* IE against global var  */
	sethi	%tie_hi22(sg1), %o3
	add	%o3, %tie_lo10(sg1), %o3
	ld	[%l1 + %o3], %o2, %tie_ld(sg1)
	add	%g7, %o2, %o4, %tie_add(sg1)
	nop;nop;nop;nop

	/* direct %g7 access IE against global var  */
	sethi	%tie_hi22(sg2), %o0
	add	%o0, %tie_lo10(sg2), %o0
	ld	[%l1 + %o0], %o0, %tie_ld(sg2)
	ld	[%g7 + %o0], %o0, %tie_add(sg2)
	nop;nop;nop;nop

	/* IE against hidden var */
	sethi	%tie_hi22(sh1), %o0
	add	%o0, %tie_lo10(sh1), %o0
	ld	[%l1 + %o0], %o0, %tie_ld(sh1)
	add	%g7, %o0, %o0, %tie_add(sh1)
	nop;nop;nop;nop

	/* direct %g7 access IE against hidden var */
	sethi	%tie_hi22(sh2), %o5
	add	%o5, %tie_lo10(sh2), %o1
	ld	[%l1 + %o1], %o2, %tie_ld(sh2)
	stb	%o3, [%g7 + %o2], %tie_add(sh2)
	nop;nop;nop;nop

	/* IE against local var  */
	sethi	%tie_hi22(bl1), %o0
	add	%o0, %tie_lo10(bl1), %o0
	ld	[%l1 + %o0], %o0, %tie_ld(bl1)
	add	%g7, %o0, %o0, %tie_add(bl1)
	nop;nop;nop;nop

	/* direct %g7 access IE against local var  */
	sethi	%tie_hi22(bl2), %o5
	add	%o5, %tie_lo10(bl2), %o1
	ld	[%l1 + %o1], %o2, %tie_ld(bl2)
	stb	%o3, [%g7 + %o2], %tie_add(bl2)
	nop;nop;nop;nop

	/* LE, local var  */
	sethi	%tle_hix22(bl3+1), %o2
	xor     %o2, %tle_lox10(bl3+1), %o4
	add	%g7, %o4, %o0
	nop;nop;nop;nop

	/* LE, hidden var, direct %g7 access  */
	sethi	%tle_hix22(sh3), %o2
	xor     %o2, %tle_lox10(sh3), %o2
	ld	[%g7 + %o2], %o2
	nop;nop;nop;nop

	ret
	restore
