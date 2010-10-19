.text
	.type _start,@function
_start:

	fclass.m p3, p4 = f4, @nat|@qnan
	fclass.m p3, p4 = f4, @nat|@qnan|@snan
	fclass.m p3, p4 = f4, @nat|@qnan|@snan|@pos
	fclass.m p3, p4 = f4, @nat|@qnan|@snan|@pos|@neg
	fclass.m p3, p4 = f4, @nat|@qnan|@snan|@pos|@neg|@unorm
	fclass.m p3, p4 = f4, @nat|@qnan|@snan|@pos|@neg|@unorm|@norm
	fclass.m p3, p4 = f4, @nat|@qnan|@snan|@pos|@neg|@unorm|@norm|@inf
