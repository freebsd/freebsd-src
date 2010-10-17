	.set	noreorder
	.set	nomacro
local:
	la	$4,late_global
	la	$4,local
	jr	$5
	la	$4,late_global
	jr	$5
	la	$4,local
	.globl	local_global
