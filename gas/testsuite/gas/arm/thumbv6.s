.text
.align 0

.thumb
label:
	cpsie  ia
	cpsid  af
	cpy    r3, r4
	rev    r2, r7
	rev16  r5, r1
	revsh  r3, r6
	setend be
	setend le
	sxth   r0, r1
	sxtb   r1, r2
	uxth   r3, r4
	uxtb   r5, r6

	# Add four nop instructions to ensure that the output is
	# 32-byte aligned as required for arm-aout.
	nop
	nop
	nop
	nop
