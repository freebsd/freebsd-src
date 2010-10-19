	lrai	gr31,gr0,#0,#0,#0
	lrai	gr0,gr31,#0,#0,#0
	lrai	gr0,gr0,#1,#0,#0
	lrai	gr0,gr0,#0,#1,#0
	lrai	gr0,gr0,#0,#0,#1

	lrad	gr31,gr0,#0,#0,#0
	lrad	gr0,gr31,#0,#0,#0
	lrad	gr0,gr0,#1,#0,#0
	lrad	gr0,gr0,#0,#1,#0
	lrad	gr0,gr0,#0,#0,#1

	tlbpr	gr31,gr0,#0,#0
	tlbpr	gr0,gr31,#0,#0
	tlbpr	gr0,gr0,#7,#0
	tlbpr	gr0,gr0,#0,#1

	mqlclrhs fr30,fr0,fr0
	mqlclrhs fr0,fr30,fr0
	mqlclrhs fr0,fr0,fr30

	mqlmths	fr30,fr0,fr0
	mqlmths	fr0,fr30,fr0
	mqlmths	fr0,fr0,fr30

	mqsllhi	fr30,#0,fr0
	mqsllhi	fr0,#63,fr0
	mqsllhi	fr0,#0,fr30

	mqsrahi	fr30,#0,fr0
	mqsrahi	fr0,#63,fr0
	mqsrahi	fr0,#0,fr30
