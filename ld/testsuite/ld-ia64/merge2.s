	.section .rodata.str1.8,"aMS", 1
.LC2:	.string "foo"
.LC1:	.string "foo"
	.section .data.rel.local,"aw"
	.quad .LC2
	.section .rodata,"a"
.LC3:	.string "bar"
	.balign 8
	.space 0x400000
	.text
	addl r12=@ltoffx(.LC1),r1 ;;
	addl r12=@ltoffx(.LC3),r1 ;;
