# Source file used to test -mips4 instructions.

text_label:	
	bc1f	text_label
	bc1f	$fcc1,text_label
	bc1fl	$fcc1,text_label
	bc1t	$fcc1,text_label
	bc1tl	$fcc2,text_label
	c.f.d	$f4,$f6
	c.f.d	$fcc1,$f4,$f6
	ldxc1	$f2,$4($5)
	lwxc1	$f2,$4($5)
	madd.d	$f0,$f2,$f4,$f6
	# This choice of arguments is so that it matches bc3f on pre-mips4.
	madd.s	$f10,$f8,$f2,$f0
	movf	$4,$5,$fcc4
	movf.d	$f4,$f6,$fcc0
	movf.s	$f4,$f6,$fcc0
	movn	$4,$6,$6
	movn.d	$f4,$f6,$6
	movn.s	$f4,$f6,$6
	movt	$4,$5,$fcc4
	movt.d	$f4,$f6,$fcc0
	movt.s	$f4,$f6,$fcc0
	movz	$4,$6,$6
	movz.d	$f4,$f6,$6
	movz.s	$f4,$f6,$6
	msub.d	$f0,$f2,$f4,$f6
	msub.s	$f0,$f2,$f4,$f6
	nmadd.d	$f0,$f2,$f4,$f6
	nmadd.s	$f0,$f2,$f4,$f6
	nmsub.d	$f0,$f2,$f4,$f6
	nmsub.s	$f0,$f2,$f4,$f6
	# It used to be disabled due to a clash with lwc3.
	pref	4,0($4)
	prefx	4,$4($5)
	recip.d	$f4,$f6
	recip.s	$f4,$f6
	rsqrt.d	$f4,$f6
	rsqrt.s	$f4,$f6
	sdxc1	$f4,$4($5)
	swxc1	$f4,$4($5)

# Round to a 16 byte boundary, for ease in testing multiple targets.
	nop
	nop
	nop
