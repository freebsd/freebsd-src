# source file to test assembly of mips64 MIPS-3D ASE instructions

	.set noreorder
	.set noat

	.text
text_label:

	addr.ps		$f4, $f8, $f19

	bc1any2f	$fcc0, text_label
	nop
	bc1any2f	$fcc2, text_label
	nop

	bc1any2t	$fcc0, text_label
	nop
	bc1any2t	$fcc4, text_label
	nop

	bc1any4f	$fcc0, text_label
	nop
	bc1any4f	$fcc4, text_label
	nop

	bc1any4t	$fcc0, text_label
	nop
	bc1any4t	$fcc4, text_label
	nop

	cabs.f.d	$fcc0, $f8, $f19
	cabs.f.d	$fcc2, $f8, $f19
	cabs.f.s	$fcc0, $f8, $f19
	cabs.f.s	$fcc2, $f8, $f19
	cabs.f.ps	$fcc0, $f8, $f19
	cabs.f.ps	$fcc2, $f8, $f19
	cabs.un.d	$fcc0, $f8, $f19
	cabs.un.d	$fcc2, $f8, $f19
	cabs.un.s	$fcc0, $f8, $f19
	cabs.un.s	$fcc2, $f8, $f19
	cabs.un.ps	$fcc0, $f8, $f19
	cabs.un.ps	$fcc2, $f8, $f19
	cabs.eq.d	$fcc0, $f8, $f19
	cabs.eq.d	$fcc2, $f8, $f19
	cabs.eq.s	$fcc0, $f8, $f19
	cabs.eq.s	$fcc2, $f8, $f19
	cabs.eq.ps	$fcc0, $f8, $f19
	cabs.eq.ps	$fcc2, $f8, $f19
	cabs.ueq.d	$fcc0, $f8, $f19
	cabs.ueq.d	$fcc2, $f8, $f19
	cabs.ueq.s	$fcc0, $f8, $f19
	cabs.ueq.s	$fcc2, $f8, $f19
	cabs.ueq.ps	$fcc0, $f8, $f19
	cabs.ueq.ps	$fcc2, $f8, $f19
	cabs.olt.d	$fcc0, $f8, $f19
	cabs.olt.d	$fcc2, $f8, $f19
	cabs.olt.s	$fcc0, $f8, $f19
	cabs.olt.s	$fcc2, $f8, $f19
	cabs.olt.ps	$fcc0, $f8, $f19
	cabs.olt.ps	$fcc2, $f8, $f19
	cabs.ult.d	$fcc0, $f8, $f19
	cabs.ult.d	$fcc2, $f8, $f19
	cabs.ult.s	$fcc0, $f8, $f19
	cabs.ult.s	$fcc2, $f8, $f19
	cabs.ult.ps	$fcc0, $f8, $f19
	cabs.ult.ps	$fcc2, $f8, $f19
	cabs.ole.d	$fcc0, $f8, $f19
	cabs.ole.d	$fcc2, $f8, $f19
	cabs.ole.s	$fcc0, $f8, $f19
	cabs.ole.s	$fcc2, $f8, $f19
	cabs.ole.ps	$fcc0, $f8, $f19
	cabs.ole.ps	$fcc2, $f8, $f19
	cabs.ule.d	$fcc0, $f8, $f19
	cabs.ule.d	$fcc2, $f8, $f19
	cabs.ule.s	$fcc0, $f8, $f19
	cabs.ule.s	$fcc2, $f8, $f19
	cabs.ule.ps	$fcc0, $f8, $f19
	cabs.ule.ps	$fcc2, $f8, $f19
	cabs.sf.d	$fcc0, $f8, $f19
	cabs.sf.d	$fcc2, $f8, $f19
	cabs.sf.s	$fcc0, $f8, $f19
	cabs.sf.s	$fcc2, $f8, $f19
	cabs.sf.ps	$fcc0, $f8, $f19
	cabs.sf.ps	$fcc2, $f8, $f19
	cabs.ngle.d	$fcc0, $f8, $f19
	cabs.ngle.d	$fcc2, $f8, $f19
	cabs.ngle.s	$fcc0, $f8, $f19
	cabs.ngle.s	$fcc2, $f8, $f19
	cabs.ngle.ps	$fcc0, $f8, $f19
	cabs.ngle.ps	$fcc2, $f8, $f19
	cabs.seq.d	$fcc0, $f8, $f19
	cabs.seq.d	$fcc2, $f8, $f19
	cabs.seq.s	$fcc0, $f8, $f19
	cabs.seq.s	$fcc2, $f8, $f19
	cabs.seq.ps	$fcc0, $f8, $f19
	cabs.seq.ps	$fcc2, $f8, $f19
	cabs.ngl.d	$fcc0, $f8, $f19
	cabs.ngl.d	$fcc2, $f8, $f19
	cabs.ngl.s	$fcc0, $f8, $f19
	cabs.ngl.s	$fcc2, $f8, $f19
	cabs.ngl.ps	$fcc0, $f8, $f19
	cabs.ngl.ps	$fcc2, $f8, $f19
	cabs.lt.d	$fcc0, $f8, $f19
	cabs.lt.d	$fcc2, $f8, $f19
	cabs.lt.s	$fcc0, $f8, $f19
	cabs.lt.s	$fcc2, $f8, $f19
	cabs.lt.ps	$fcc0, $f8, $f19
	cabs.lt.ps	$fcc2, $f8, $f19
	cabs.nge.d	$fcc0, $f8, $f19
	cabs.nge.d	$fcc2, $f8, $f19
	cabs.nge.s	$fcc0, $f8, $f19
	cabs.nge.s	$fcc2, $f8, $f19
	cabs.nge.ps	$fcc0, $f8, $f19
	cabs.nge.ps	$fcc2, $f8, $f19
	cabs.le.d	$fcc0, $f8, $f19
	cabs.le.d	$fcc2, $f8, $f19
	cabs.le.s	$fcc0, $f8, $f19
	cabs.le.s	$fcc2, $f8, $f19
	cabs.le.ps	$fcc0, $f8, $f19
	cabs.le.ps	$fcc2, $f8, $f19
	cabs.ngt.d	$fcc0, $f8, $f19
	cabs.ngt.d	$fcc2, $f8, $f19
	cabs.ngt.s	$fcc0, $f8, $f19
	cabs.ngt.s	$fcc2, $f8, $f19
	cabs.ngt.ps	$fcc0, $f8, $f19
	cabs.ngt.ps	$fcc2, $f8, $f19

	cvt.pw.ps	$f4, $f19

	cvt.ps.pw	$f4, $f19

	mulr.ps		$f4, $f8, $f19

	recip1.d	$f8, $f19
	recip1.s	$f8, $f19
	recip1.ps	$f8, $f19

	recip2.d	$f4, $f8, $f19
	recip2.s	$f4, $f8, $f19
	recip2.ps	$f4, $f8, $f19

	rsqrt1.d	$f8, $f19
	rsqrt1.s	$f8, $f19
	rsqrt1.ps	$f8, $f19

	rsqrt2.d	$f4, $f8, $f19
	rsqrt2.s	$f4, $f8, $f19
	rsqrt2.ps	$f4, $f8, $f19

	bc1any2f	$fcc1, text_label	# warns
	nop
	bc1any2t	$fcc3, text_label	# warns
	nop
	bc1any4f	$fcc1, text_label	# warns
	nop
	bc1any4t	$fcc2, text_label	# warns
	nop

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
      .space  8
