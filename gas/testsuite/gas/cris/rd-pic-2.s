; GAS mustn't error on the larger-than-16-bit offsets here.

	.global y
	.global z
a:
	movs.w y:GOT16,$r9
	movs.w z:GOTPLT16,$r9
	.space 65536,0
y:
	nop
z:
