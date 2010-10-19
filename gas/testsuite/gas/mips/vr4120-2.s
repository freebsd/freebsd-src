# Test workarounds selected by -mfix-vr4120.
# Note that we only work around bugs gcc may generate.

r21:
	macc	$4,$5,$6
	div	$0,$7,$8
	or	$4,$5

	dmacc	$4,$5,$6
	div	$0,$7,$8
	or	$4,$5

	macc	$4,$5,$6
	divu	$0,$7,$8
	or	$4,$5

	dmacc	$4,$5,$6
	divu	$0,$7,$8
	or	$4,$5

	macc	$4,$5,$6
	ddiv	$0,$7,$8
	or	$4,$5

	dmacc	$4,$5,$6
	ddiv	$0,$7,$8
	or	$4,$5

	macc	$4,$5,$6
	ddivu	$0,$7,$8
	or	$4,$5

	dmacc	$4,$5,$6
	ddivu	$0,$7,$8
	or	$4,$5

r23:
	dmult	$4,$5
	dmult	$6,$7
	or	$4,$5

	dmultu	$4,$5
	dmultu	$6,$7
	or	$4,$5

	dmacc	$4,$5,$6
	dmacc	$6,$7,$8
	or	$4,$5

	dmult	$4,$5
	dmacc	$6,$7,$8
	or	$4,$5

r24:
	macc	$4,$5,$6
	mtlo	$7

	dmacc	$4,$5,$6
	mtlo	$7

	macc	$4,$5,$6
	mthi	$7

	dmacc	$4,$5,$6
	mthi	$7

vr4181a_md1:
	macc	$4,$5,$6
	mult	$4,$5
	or	$4,$5

	macc	$4,$5,$6
	multu	$4,$5
	or	$4,$5

	macc	$4,$5,$6
	dmult	$4,$5
	or	$4,$5

	macc	$4,$5,$6
	dmultu	$4,$5
	or	$4,$5

	dmacc	$4,$5,$6
	mult	$4,$5
	or	$4,$5

	dmacc	$4,$5,$6
	multu	$4,$5
	or	$4,$5

	dmacc	$4,$5,$6
	dmult	$4,$5
	or	$4,$5

	dmacc	$4,$5,$6
	dmultu	$4,$5
	or	$4,$5

vr4181a_md4:
	dmult	$4,$5
	macc	$4,$5,$6
	or	$4,$5

	dmultu	$4,$5
	macc	$4,$5,$6
	or	$4,$5

	div	$0,$4,$5
	macc	$4,$5,$6
	or	$4,$5

	divu	$0,$4,$5
	macc	$4,$5,$6
	or	$4,$5

	ddiv	$0,$4,$5
	macc	$4,$5,$6
	or	$4,$5

	ddivu	$0,$4,$5
	macc	$4,$5,$6
	or	$4,$5

	dmult	$4,$5
	dmacc	$4,$5,$6
	or	$4,$5

	dmultu	$4,$5
	dmacc	$4,$5,$6
	or	$4,$5

	div	$0,$4,$5
	dmacc	$4,$5,$6
	or	$4,$5

	divu	$0,$4,$5
	dmacc	$4,$5,$6
	or	$4,$5

	ddiv	$0,$4,$5
	dmacc	$4,$5,$6
	or	$4,$5

	ddivu	$0,$4,$5
	dmacc	$4,$5,$6
	or	$4,$5
