	.set	noreorder
	.set	nomacro
early_big:
	la	$4,early_big
	la	$4,sdata
	jr	$5
	la	$4,early_big
	jr	$5
	la	$4,sdata
	.comm	sdata,4
