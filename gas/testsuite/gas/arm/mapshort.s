	.text
	.type foo, %function
foo:
	.code 32
	nop
	.code 16
	nop
	nop
	.long 2
	.short 1
	.short 1
	.short 3
	nop
	nop
	.short 1
	.code 32
	bl foo
	.short 8
	.byte 9
bar:
	.byte 10
	.data
wibble:
	.word 0
