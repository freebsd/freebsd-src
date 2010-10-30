	.section .rodata.str1.8,"aMS", 1
.LC1:	.string "foo"
.LC2:	.string "foo"
.LC3:	.string "bar"
.LC4:	.string "bar"
.LC5:	.string "bar"
.LC6:	.string "bar"
.LC7:	.string "baz"
.LC8:	.string "baz"
	.section .data.rel.local,"aw"
	.quad .LC2
	.quad .LC4
	.quad .LC6
	.quad .LC7
	.section .rodata,"a"
.LC9:	.string "mumble"
	.balign 8
	.space 0x400000
	.text
	addl r12=@ltoffx(.LC1),r1 ;;
	addl r12=@ltoffx(.LC3),r1 ;;
	addl r12=@ltoffx(.LC5),r1 ;;
	addl r12=@ltoffx(.LC8),r1 ;;
	addl r12=@ltoffx(.LC9),r1 ;;
