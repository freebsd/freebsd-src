	.text
	.global foo
	.global g_name
	.align 4
foo:
	entry a5,16
	movi a5,20000
	movi a6,g_name
	movi a7,50000
	ret
