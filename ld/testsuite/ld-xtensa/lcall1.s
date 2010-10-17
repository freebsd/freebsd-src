.global foo
.text
	.align 4
label1:
	.begin literal
	.word 0xffffffff
	.end literal
	entry a5,16
.begin longcalls
	call4  foo
.end longcalls
	nop
