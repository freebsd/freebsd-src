	.section .rodata.str,"aMS","progbits",1
.LC0:
	.asciz	"abc"
.LC1:
	.asciz	"c"
.LC2:
	.asciz	"bc"
.LC3:
	.asciz	"b"


	.section .rodata.str2,"aMS","progbits",4
	.p2align 2
.LC4:
	.long	0x12345678
	.long	0
.LC5:
	.long	0x12345678
	.long	0x99999999
	.long	0x12345678
	.long	0
.LC6:
	.long	0x99999999
	.long	0
.LC7:
	.long	0x99999999
	.long	0x12345678
	.long	0


	.section .rodata.m,"aM","progbits",4
	.p2align 2
.LC8:
	.long	0x12345678
.LC9:
	.long	0x99999999
.LC10:
	.long	0
.LC11:
	.long	0x12345678


	.text
	.global _start
_start:
	.long	.LC0
.LT0:
	.long	.LC1
	.long	.LC2
	.long	.LC3
	.long	.LC4
	.long	.LC5
	.long	.LC6
	.long	.LC7
	.long	.LC8
	.long	.LC9
	.long	.LC10
	.long	.LC11
