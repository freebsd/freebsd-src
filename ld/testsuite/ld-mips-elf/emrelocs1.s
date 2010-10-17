# emrelocs1.s: some external symbols to be used in relocations.

		.text
		.p2align 4

		# Pad things so addresses which are used for relocations
		# are non-zero.  Zero simply isn't as much fun.
		.word	0

		.globl ext_fun
		.ent ext_fun
ext_fun:	.word	1
		.end ext_fun


		.sdata
		.p2align 4

		# Padding here, for same reason.
		.word	0

		.globl	ext_var
ext_var:	.word	2
