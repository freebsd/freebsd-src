	.text
	.global	_convert_from_extended

_convert_from_extended:

	ldfe	f0,[a1]
	stfd	f0,[a2]
	movs	pc,lr

	.global	_convert_to_extended

_convert_to_extended:

	ldfd	f0,[a1]
	stfe	f0,[a2]
	movs	pc,lr
