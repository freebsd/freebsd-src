	.text
	.global foo
	.global g_name
foo:
	entry a5,16
	movi a5,20000
	movi a6,g_name
	movi a7,50000
	ret
