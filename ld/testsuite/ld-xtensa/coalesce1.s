	.global foo
	.data
	.global g_name
	.align 4
g_name:
	.word 0xffffffff
	.text
	.global main
	.align 4
main:
	entry a5,16
	movi a5,20000
	movi a6,g_name
	call8 foo
	ret
