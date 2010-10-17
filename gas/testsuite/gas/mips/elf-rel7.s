	.global	frob
	.section .barsec,"aM",@progbits,8
	.word	0,1
bar:	.word	2,3
frob:	.word	4,5
	.text
foo:	lw	$4,bar
	lw	$4,bar+4
	lw	$4,bar+8
	lw	$4,frob
	lw	$4,frob+4
	lw	$4,frob+16
