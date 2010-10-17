	.data
	.align	4096
	.section ".tdata", #alloc, #write, #tls
	.align	4
	.globl sg1, sg2, sg3, sg4, sg5, sg6, sg7, sg8
	.globl sh1, sh2, sh3, sh4, sh5, sh6, sh7, sh8
	.hidden sh1, sh2, sh3, sh4, sh5, sh6, sh7, sh8
sg1:	.word 17
	.skip	4096
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
        add     %o7, %l7, %l7

	.globl	fn2
	.type	fn2,#function
	.proc	04
fn2:
	save	%sp, -104, %sp
	sethi	%hi(_GLOBAL_OFFSET_TABLE_-4), %l7
	call	.LLGETPC0
	add	%l7, %lo(_GLOBAL_OFFSET_TABLE_+4), %l7
	nop;nop;nop;nop

	/* GD -> IE because variable is not defined in executable */
	sethi	%tgd_hi22(sG1), %l1
	nop
	add	%l1, %tgd_lo10(sG1), %l2
	nop
	add	%l7, %l2, %o0, %tgd_add(sG1)
	nop
	call	__tls_get_addr, %tgd_call(sG1)
	nop
	nop;nop;nop;nop

	/* GD -> IE because variable is not defined in executable where
	   the variable is referenced through IE too */
	sethi	%tgd_hi22(sG2), %o0
	add	%o0, %tgd_lo10(sG2), %o1
	add	%l7, %o1, %o0, %tgd_add(sG2)
	call	__tls_get_addr, %tgd_call(sG2)
	nop
	nop;nop;nop;nop

	/* GD -> LE with global variable defined in executable */
	sethi	%tgd_hi22(sg1), %l0
	add	%l0, %tgd_lo10(sg1), %l5
	add	%l7, %l5, %o0, %tgd_add(sg1)
	call	__tls_get_addr, %tgd_call(sg1)
	nop
	nop;nop;nop;nop

	/* GD -> LE with local variable defined in executable */
	sethi	%tgd_hi22(sl1), %o0
	add	%o0, %tgd_lo10(sl1), %o1
	add	%l7, %o1, %o0, %tgd_add(sl1)
	call	__tls_get_addr, %tgd_call(sl1)
	nop
	nop;nop;nop;nop

	/* GD -> LE with hidden variable defined in executable */
	sethi	%tgd_hi22(sh1), %o0
	add	%o0, %tgd_lo10(sh1), %o1
	add	%l7, %o1, %o0, %tgd_add(sh1)
	call	__tls_get_addr, %tgd_call(sh1)
	nop
	nop;nop;nop;nop

	/* LD -> LE */
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

	/* LD -> LE against hidden variables */
	sethi	%tldm_hi22(sh1), %o1
	sethi	%tldo_hix22(sh1), %l3
	add	%o1, %tldm_lo10(sh1), %o2
	sethi	%tldo_hix22(sh2 + 1), %l2
	add	%l7, %o2, %o0, %tldm_add(sh1)
	xor	%l3, %tldo_lox10(sh1), %l4
	call	__tls_get_addr, %tldm_call(sh1)
	xor	%l2, %tldo_lox10(sh2 + 1), %l3
	add	%o0, %l4, %l5, %tldo_add(sh1)
	ldub	[%o0 + %l3], %l6, %tldo_add(sh2 + 1)
	nop;nop;nop;nop

	/* IE against global var  */
	sethi	%tie_hi22(sG2), %l1
	nop
	add	%l1, %tie_lo10(sG2), %l2
	nop
	ld	[%l7 + %l2], %l2, %tie_ld(sG2)
	nop
	add	%g7, %l2, %l2, %tie_add(sG2)
	nop;nop;nop;nop

	/* IE -> LE against global var defined in exec */
	sethi	%tie_hi22(sg1), %o3
	add	%o3, %tie_lo10(sg1), %o3
	ld	[%l7 + %o3], %o3, %tie_ld(sg1)
	add	%g7, %o3, %o4, %tie_add(sg1)
	nop;nop;nop;nop

	/* IE -> LE against local var */
	sethi	%tie_hi22(sl1), %l4
	add	%l4, %tie_lo10(sl1), %l1
	ld	[%l7 + %l1], %l3, %tie_ld(sl1)
	add	%g7, %l3, %l3, %tie_add(sl1)
	nop;nop;nop;nop

	/* IE -> LE against hidden var */
	sethi	%tie_hi22(sh1), %o1
	add	%o1, %tie_lo10(sh1), %o3
	ld	[%l7 + %o3], %o0, %tie_ld(sh1)
	add	%g7, %o0, %o3, %tie_add(sh1)
	nop;nop;nop;nop

	/* Direct access through %g7  */

	/* IE against global var  */
	sethi	%tie_hi22(sG5), %o3
	add	%o3, %tie_lo10(sG5), %o3
	ld	[%l7 + %o3], %o2, %tie_ld(sG5)
	ld	[%g7 + %o2], %o4, %tie_add(sG5)
	nop;nop;nop;nop

	/* IE->LE against local var  */
	sethi	%tie_hi22(sl5), %o3
	add	%o3, %tie_lo10(sl5), %o3
	ld	[%l7 + %o3], %o2, %tie_ld(sl5)
	stb	%o4, [%g7 + %o2], %tie_add(sl5)
	nop;nop;nop;nop

	/* IE->LE against hidden var  */
	sethi	%tie_hi22(sh5), %o5
	add	%o5, %tie_lo10(sh5), %o3
	ld	[%l7 + %o3], %o2, %tie_ld(sh5)
	ldsb	[%g7 + %o2], %o4, %tie_add(sh5)
	nop;nop;nop;nop

	ret
	restore
