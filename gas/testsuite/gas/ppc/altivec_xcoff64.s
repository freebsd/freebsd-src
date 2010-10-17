# PowerPC xcoff64 AltiVec tests
#as: -a64 -mppc64 -maltivec
	.machine	"ppc64"
	.csect .text[PR]
	.csect main[DS]
main:
	.csect .text[PR]
.main:
	dss	3
	dssall
	dst	5,4,1
	dstt	8,7,0
	dstst	5,6,3
	dststt	4,5,2
