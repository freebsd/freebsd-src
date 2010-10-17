
	.text
	.global compare
compare:
	cmp r5,r6
	cmp 5,r6
	setf v,r5
	setf nv,r5
	setf c,r5
	setf l,r5
	setf nc,r5
	setf nl,r5
	setf z,r5
	setf nz,r5
	setf nh,r5
	setf h,r5
	setf s,r5
	setf n,r5
	setf ns,r5
	setf p,r5
	setf t,r5
	setf sa,r5
	setf lt,r5
	setf ge,r5
	setf le,r5
	setf gt,r5
	tst r5,r6
	
