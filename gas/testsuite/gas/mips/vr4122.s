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
