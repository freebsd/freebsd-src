	.data
	.global expobj
	.type	expobj,@object
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

