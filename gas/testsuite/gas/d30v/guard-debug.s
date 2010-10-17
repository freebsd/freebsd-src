# Same as guard.s but here we are testing debug (-g) assembly
# On the D30V, assembling with -g should disable the VLIW packing
# and put only one instruction per line.	

	.text

	add	r1,r2,r3
	add/al	r1,r2,r3
	add/tx	r1,r2,r3
	add/fx	r1,r2,r3
	add/xt	r1,r2,r3
	add/xf	r1,r2,r3
	add/tt	r1,r2,r3
	add/tf	r1,r2,r3

	

