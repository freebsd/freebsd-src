	.sdata
	.global	foo
	.globl	bar
foo:	.byte	1
	.byte	2
bar:	.byte	3
	.byte	4

	.text
	.set mips16
	.global	f
	.ent	f
f:
	lb	$2,%gprel(bar)($3)
	lb	$2,%gprel(bar+1)($3)
	.end	f

# align section end to 16-byte boundary for easier testing on multiple targets
	.p2align 4
