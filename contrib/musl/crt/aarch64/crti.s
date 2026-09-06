.section .init
.global _init
.type _init,%function
.align 2
_init:
	stp x29,x30,[sp,-16]!
	mov x29,sp

.section .fini
.global _fini
.type _fini,%function
.align 2
_fini:
	stp x29,x30,[sp,-16]!
	mov x29,sp
