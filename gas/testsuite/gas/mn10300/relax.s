	.am33_2
	
	.section .rlcb, "ax"
	.global relax_long_cond_branch
relax_long_cond_branch:
	clr d0
	clr d1
.L1:	
	add d1,d0
	inc d1

	.fill 32764, 1, 0xcb

	cmp 9,d1
	ble .L1
	rets
	

	.section .rlfcb, "ax"
	.global relax_long_float_cond_branch
relax_long_float_cond_branch:
	clr d0
	clr d1
.L2:
	add d1,d0
	inc d1

	.fill 32764, 1, 0xcb

	cmp 9,d1
	fble .L2
	rets

	.section .rscb, "ax"
	.global relax_short_cond_branch
relax_short_cond_branch:
	clr d0
	clr d1
.L3:
	add d1,d0
	inc d1

	.fill 252, 1, 0xcb

	cmp 9,d1
	ble .L3
	rets

	.section .rsfcb, "ax"
	.global relax_short_float_cond_branch
relax_short_float_cond_branch:
	clr d0
	clr d1
.L4:
	add d1,d0
	inc d1

	.fill 252, 1, 0xcb

	cmp 9,d1
	fble .L4
	rets
	
	.section .rsucb, "ax"
	.global relax_short_uncommon_cond_branch
relax_short_uncommon_cond_branch:
	clr d0
	clr d1
.L5:
	add d1,d0
	inc d1

	.fill 252, 1, 0xcb

	cmp 9,d1
	bvc .L5
	rets

	.section .rlucb, "ax"
	.global relax_long_uncommon_cond_branch
relax_long_uncommon_cond_branch:
	clr d0
	clr d1
.L6:
	add d1,d0
	inc d1

	.fill 32764, 1, 0xcb

	cmp 9,d1
	bvc .L6
	rets

