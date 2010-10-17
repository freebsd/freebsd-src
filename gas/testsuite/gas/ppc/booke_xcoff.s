# Motorola PowerPC BookE tests
#as: -mppc32 -mbooke32
	.machine	"ppc32"
	.csect .text[PR]
	.csect main[DS]
main:
	.csect .text[PR]
.main:

	tlbre   1, 2, 7
	tlbwe   5, 30, 3
	icbt	5, 8, 9	
	mfapidi	5, 6
	tlbivax	7, 8
	tlbivaxe 9, 10	
	tlbsx	11, 12
	tlbsxe	13, 14
	mcrxr64 5
	rfci
	wrtee	3
	wrteei	1
	mfdcrx	4, 5
	mfdcr	5, 234
	mtdcrx	6, 7
	mtdcr	432, 8
	msync
	dcba	9, 10
	mbar	0
