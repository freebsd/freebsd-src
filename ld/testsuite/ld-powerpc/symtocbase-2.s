.section .toc,"aw",@progbits
	.align 15
	.globl y
y:	.quad	.y,.y@tocbase,0
.LCi:	.quad	i
	.space	48 * 1024
.data
.L2bases:
	.quad	.TOC.@tocbase
	.quad	.x@tocbase
	.quad	.y@tocbase
.text
	.globl .y
.y:
	ld 9,.LCi@toc(2)
	blr
