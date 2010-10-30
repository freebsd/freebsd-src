	.section .rodata.str1.8,"aMS", 1
.LC1:	.string "foo"
.LC2:	.string "foo"
.LC3:	.string "bar"
.LC4:	.string "bar"
	.section .data.rel.local,"aw"
	.quad .LC2
	.quad .LC3
	.section .rodata,"a"
.LC5:	.string "mumble"
	.balign 8
	.space 0x400000
	.text
	addl r12=@ltoffx(.LC1),r1 ;;
	addl r12=@ltoffx(.LC4),r1 ;;
	addl r12=@ltoffx(.LC5),r1 ;;
