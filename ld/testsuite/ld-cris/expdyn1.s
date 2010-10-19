	.data
	.global expobj
	.type	expobj,@object
	.size	expobj,4
expobj:
	.dword 0

	.text
	.global _start
_start:
	nop
	.global expfn
expfn:
	.type	expfn,@function
	nop
.Lfe1:
	.size	expfn,.Lfe1-expfn

