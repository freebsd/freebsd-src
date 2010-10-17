# D30V guarded execution assembly test

	.text

	add	r1,r2,r3
	add/al	r1,r2,r3
	add/tx	r1,r2,r3
	add/fx	r1,r2,r3
	add/xt	r1,r2,r3
	add/xf	r1,r2,r3
	add/tt	r1,r2,r3
	add/tf	r1,r2,r3

# check case sensitivity too
	ADD	r1,r2,r3
	ADD/AL	r1,r2,r3
	ADD/tx	r1,r2,r3
	add/FX	r1,r2,r3
	ADD/XT	r1,r2,r3
	ADD/XF	r1,r2,r3
	add/TT	r1,r2,r3
	ADD/tf	r1,r2,r3
	

