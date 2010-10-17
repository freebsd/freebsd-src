# PowerPC AltiVec tests
#as: -m601 -maltivec
	.section ".text"
start:
	dss	3
	dssall	
	dst	5,4,1
	dstt	8,7,0
	dstst	5,6,3
	dststt	4,5,2
