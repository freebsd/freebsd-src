	la	$4,local
	la	$4,global
	lw	$4,local
	lw	$4,global
	sw	$4,local
	sw	$4,global
	ulw	$4,local
	ulw	$4,global
	usw	$4,local
	usw	$4,global
	.space	16

	.data
	.global global
local:	.word	4
global:	.word	8
