// Make sure error messages on 'alloc' don't needlessly refer to operand 1
// (which gets parsed late) when only one of the other operands is wrong.

	.text

alloc:
	alloc		r2 = ar.pfs, x, 0, 0, 0
	alloc		r2 = ar.pfs, 0, x, 0, 0
	alloc		r2 = ar.pfs, 0, 0, x, 0
	alloc		r2 = ar.pfs, 0, 0, 0, x
	alloc		r3 = x, 0, 0, 0
	alloc		r3 = 0, x, 0, 0
	alloc		r3 = 0, 0, x, 0
	alloc		r3 = 0, 0, 0, x
