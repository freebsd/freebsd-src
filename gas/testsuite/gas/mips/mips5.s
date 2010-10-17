# Source file used to test -mips5 instructions.

text_label:	
	abs.ps		$f0, $f2
	add.ps		$f2, $f4, $f6
	alnv.ps		$f6, $f8, $f10, $3
	c.eq.ps		$f8, $f10
	c.eq.ps		$fcc2, $f10, $f12
	c.f.ps	    	$f8, $f10
	c.f.ps	    	$fcc2, $f10, $f12
	c.le.ps		$f8, $f10
	c.le.ps		$fcc2, $f10, $f12
	c.lt.ps		$f8, $f10
	c.lt.ps		$fcc2, $f10, $f12
	c.nge.ps	$f8, $f10
	c.nge.ps	$fcc2, $f10, $f12
	c.ngl.ps	$f8, $f10
	c.ngl.ps	$fcc2, $f10, $f12
	c.ngle.ps	$f8, $f10
	c.ngle.ps	$fcc2, $f10, $f12
	c.ngt.ps	$f8, $f10
	c.ngt.ps	$fcc2, $f10, $f12
	c.ole.ps	$f8, $f10
	c.ole.ps	$fcc2, $f10, $f12
	c.olt.ps	$f8, $f10
	c.olt.ps	$fcc2, $f10, $f12
	c.seq.ps	$f8, $f10
	c.seq.ps	$fcc2, $f10, $f12
	c.sf.ps		$f8, $f10
	c.sf.ps		$fcc2, $f10, $f12
	c.ueq.ps	$f8, $f10
	c.ueq.ps	$fcc2, $f10, $f12
	c.ule.ps	$f8, $f10
	c.ule.ps	$fcc2, $f10, $f12
	c.ult.ps	$f8, $f10
	c.ult.ps	$fcc2, $f10, $f12
	c.un.ps		$f8, $f10
	c.un.ps		$fcc2, $f10, $f12
	cvt.ps.s	$f12, $f14, $f16
	cvt.s.pl	$f16, $f18
	cvt.s.pu	$f18, $f20
	luxc1		$f20, $4($5)
	madd.ps		$f20, $f22, $f24, $f26
	mov.ps		$f24, $f26
	movf.ps		$f26, $f28, $fcc2
	movn.ps		$f26, $f28, $3
	movt.ps		$f28, $f30, $fcc4
	movz.ps		$f28, $f30, $5
	msub.ps		$f30, $f0, $f2, $f4
	mul.ps		$f2, $f4, $f6
	neg.ps		$f6, $f8
	nmadd.ps	$f6, $f8, $f10, $f12
	nmsub.ps	$f6, $f8, $f10, $f12
	pll.ps		$f10, $f12, $f14
	plu.ps		$f14, $f16, $f18
	pul.ps		$f16, $f18, $f20
	puu.ps		$f20, $f22, $f24
	sub.ps		$f22, $f24, $f26
	suxc1		$f26, $6($7)

	c.eq.ps		$fcc3, $f10, $f12	# warns
	movf.ps		$f26, $f28, $fcc3	# warns

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
      .space  8
