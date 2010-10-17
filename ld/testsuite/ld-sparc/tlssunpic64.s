	.data
	.align	4096
	.section ".tdata", #alloc, #write, #tls
	.align	4
	.globl sg1, sg2, sg3, sg4, sg5, sg6, sg7, sg8
	.globl sh1, sh2, sh3, sh4, sh5, sh6, sh7, sh8
	.hidden sh1, sh2, sh3, sh4, sh5, sh6, sh7, sh8
sg1:	.word 17
sg2:	.word 18
sg3:	.word 19
sg4:	.word 20
sg5:	.word 21
sg6:	.word 22
sg7:	.word 23
sg8:	.word 24
sl1:	.word 65
sl2:	.word 66
sl3:	.word 67
sl4:	.word 68
sl5:	.word 69
sl6:	.word 70
sl7:	.word 71
sl8:	.word 72
sh1:	.word 257
sh2:	.word 258
sh3:	.word 259
sh4:	.word 260
sh5:	.word 261
sh6:	.word 262
sh7:	.word 263
sh8:	.word 264

	.text
	.align	4096
.LLGETPC0:
	retl
	add	%o7, %l7, %l7

	.globl	fn1
	.type	fn1,#function
	.proc	04
fn1:
	save	%sp, -160, %sp
	sethi	%hi(_GLOBAL_OFFSET_TABLE_-4), %l7
	call	.LLGETPC0
	add	%l7, %lo(_GLOBAL_OFFSET_TABLE_+4), %l7
	nop;nop;nop;nop

	/* GD */
	sethi	%tgd_hi22(sg1), %l1
	nop
	add	%l1, %tgd_lo10(sg1), %l2
	nop
	add	%l7, %l2, %o0, %tgd_add(sg1)
	nop
	call	__tls_get_addr, %tgd_call(sg1)
	nop
	nop;nop;nop;nop

	/* GD -> IE because variable is referenced through IE too */
	sethi	%tgd_hi22(sg2), %o0
	add	%o0, %tgd_lo10(sg2), %o1
	add	%l7, %o1, %o0, %tgd_add(sg2)
	call	__tls_get_addr, %tgd_call(sg2)
	nop
	nop;nop;nop;nop

	/* GD against local variable */
	sethi	%tgd_hi22(sl1), %o4
	add	%o4, %tgd_lo10(sl1), %o4
	add	%l7, %o4, %o0, %tgd_add(sl1)
	call	__tls_get_addr, %tgd_call(sl1)
	nop
	nop;nop;nop;nop

	/* GD -> IE against local variable referenced through IE too */
	sethi	%tgd_hi22(sl2), %o0
	add	%o0, %tgd_lo10(sl2), %o0
	add	%l7, %o0, %o0, %tgd_add(sl2)
	call	__tls_get_addr, %tgd_call(sl2)
	nop
	nop;nop;nop;nop

	/* GD against hidden and local variable */
	sethi	%tgd_hi22(sh1), %o4
	add	%o4, %tgd_lo10(sh1), %o4
	add	%l7, %o4, %o0, %tgd_add(sh1)
	call	__tls_get_addr, %tgd_call(sh1)
	nop
	nop;nop;nop;nop

	/* GD -> IE against hidden and local variable referenced through
	   IE too */
	sethi	%tgd_hi22(sh2), %o0
	add	%o0, %tgd_lo10(sh2), %o0
	add	%l7, %o0, %o0, %tgd_add(sh2)
	call	__tls_get_addr, %tgd_call(sh2)
	nop
	nop;nop;nop;nop

	/* GD against hidden but not local variable */
	sethi	%tgd_hi22(sH1), %o4
	add	%o4, %tgd_lo10(sH1), %o4
	add	%l7, %o4, %o0, %tgd_add(sH1)
	call	__tls_get_addr, %tgd_call(sH1)
	nop
	nop;nop;nop;nop

	/* GD -> IE against hidden but not local variable referenced through
	   IE too */
	sethi	%tgd_hi22(sH2), %o0
	add	%o0, %tgd_lo10(sH2), %o0
	add	%l7, %o0, %o0, %tgd_add(sH2)
	call	__tls_get_addr, %tgd_call(sH2)
	nop
	nop;nop;nop;nop

	/* LD */
	sethi	%tldm_hi22(sl1), %l1
	nop
	add	%l1, %tldm_lo10(sl1), %l2
	nop
	add	%l7, %l2, %o0, %tldm_add(sl1)
	nop
	call	__tls_get_addr, %tldm_call(sl1)
	nop
	sethi	%tldo_hix22(sl1), %l3
	nop
	xor	%l3, %tldo_lox10(sl1), %l4
	nop
	add	%o0, %l4, %l5, %tldo_add(sl1)
	nop
	sethi	%tldo_hix22(sl2 + 2), %l2
	nop
	xor	%l2, %tldo_lox10(sl2 + 2), %l3
	nop
	lduh	[%o0 + %l3], %l6, %tldo_add(sl2 + 2)
	nop;nop;nop;nop

	/* LD against hidden and local variables */
	sethi	%tldm_hi22(sh1), %o1
	sethi	%tldo_hix22(sh1), %l3
	add	%o1, %tldm_lo10(sh1), %o2
	sethi	%tldo_hix22(sh2 + 1), %l2
	add	%l7, %o2, %o0, %tldm_add(sh1)
	xor	%l3, %tldo_lox10(sh1), %l4
	call	__tls_get_addr, %tldm_call(sh1)
	xor	%l2, %tldo_lox10(sh2 + 1), %l3
	ldx	[%o0 + %l4], %l5, %tldo_add(sh1)
	add	%o0, %l3, %l6, %tldo_add(sh2 + 1)
	nop;nop;nop;nop

	/* LD against hidden but not local variables */
	sethi	%tldm_hi22(sH1), %o1
	sethi	%tldo_hix22(sH1 + 3), %l3
	add	%o1, %tldm_lo10(sH1), %o2
	sethi	%tldo_hix22(sH2), %l2
	add	%l7, %o2, %o0, %tldm_add(sH1)
	xor	%l3, %tldo_lox10(sH1 + 3), %l4
	call	__tls_get_addr, %tldm_call(sH1)
	xor	%l2, %tldo_lox10(sH2), %l3
	add	%o0, %l4, %l5, %tldo_add(sH1 + 3)
	ld	[%o0 + %l3], %l6, %tldo_add(sH2)
	nop;nop;nop;nop

	/* IE against global var  */
	sethi	%tie_hi22(sg2), %l1
	nop
	add	%l1, %tie_lo10(sg2), %l2
	nop
	ldx	[%l7 + %l2], %l2, %tie_ldx(sg2)
	nop
	add	%g7, %l2, %l2, %tie_add(sg2)
	nop;nop;nop;nop

	/* IE against local var  */
	sethi	%tie_hi22(sl2), %o3
	add	%o3, %tie_lo10(sl2), %o3
	ldx	[%l7 + %o3], %o2, %tie_ldx(sl2)
	add	%g7, %o2, %o4, %tie_add(sl2)
	nop;nop;nop;nop

	/* IE against hidden and local var  */
	sethi	%tie_hi22(sh2), %l1
	add	%l1, %tie_lo10(sh2), %l2
	ldx	[%l7 + %l2], %l2, %tie_ldx(sh2)
	add	%g7, %l2, %l2, %tie_add(sh2)
	nop;nop;nop;nop

	/* IE against hidden but not local var  */
	sethi	%tie_hi22(sH2), %l1
	add	%l1, %tie_lo10(sH2), %l2
	ldx	[%l7 + %l2], %l2, %tie_ldx(sH2)
	add	%g7, %l2, %l2, %tie_add(sH2)
	nop;nop;nop;nop

	/* Direct access through %g7  */

	/* IE against global var  */
	sethi	%tie_hi22(sg5), %l1
	add	%l1, %tie_lo10(sg5), %l2
	ldx	[%l7 + %l2], %l2, %tie_ldx(sg5)
	ldx	[%g7 + %l2], %l2, %tie_add(sg5)
	nop;nop;nop;nop

	/* IE against local var  */
	sethi	%tie_hi22(sl5), %o3
	add	%o3, %tie_lo10(sl5), %o4
	ldx	[%l7 + %o4], %o5, %tie_ldx(sl5)
	stb	%l2, [%g7 + %o5], %tie_add(sl5)
	nop;nop;nop;nop

	/* IE against hidden and local var  */
	sethi	%tie_hi22(sh5), %o3
	add	%o3, %tie_lo10(sh5), %o4
	ldx	[%l7 + %o4], %o5, %tie_ldx(sh5)
	stx	%l2, [%g7 + %o5], %tie_add(sh5)
	nop;nop;nop;nop

	/* IE against hidden but not local var  */
	sethi	%tie_hi22(sH5), %o3
	add	%o3, %tie_lo10(sH5), %o4
	ldx	[%l7 + %o4], %o5, %tie_ldx(sH5)
	st	%l2, [%g7 + %o5], %tie_add(sH5)
	nop;nop;nop;nop

	return	%i7 + 8
	nop
