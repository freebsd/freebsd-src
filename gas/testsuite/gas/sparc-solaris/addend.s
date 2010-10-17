	.global foo
foo:
	nop
	nop
	ba foo1+0x4
	ba foo1+0x4
	ba foo1
	ba foo1
	nop
	.word foo1
	.word foo1+4
