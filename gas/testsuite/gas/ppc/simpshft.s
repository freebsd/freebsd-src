# These are all the examples from section F.4 of
# "PowerPC Microprocessor Family: The Programming Environments".
# 64-bit examples
	extrdi	%r4,%r3,1,0
	insrdi	%r3,%r4,1,0
	sldi	%r5,%r5,8
	clrldi	%r4,%r3,32
# 32-bit examples
	extrwi	%r4,%r3,1,0
	insrwi	%r3,%r4,1,0
	slwi	%r5,%r5,8
	clrlwi	%r4,%r3,16


# These test the remaining corner cases for 64-bit operations.
	extldi	%r4,%r3,1,0
	extldi	%r4,%r3,64,0
	extldi	%r4,%r3,1,63
	extldi	%r4,%r3,64,63    # bit weird, that one.
	
	extrdi	%r4,%r3,63,0
	extrdi	%r4,%r3,1,62

	insrdi	%r4,%r3,64,0
	insrdi	%r4,%r3,63,0
	insrdi	%r4,%r3,1,62
	insrdi	%r4,%r3,1,63

	rotldi	%r4,%r3,0
	rotldi	%r4,%r3,1
	rotldi	%r4,%r3,63

	rotrdi	%r4,%r3,0
	rotrdi	%r4,%r3,1
	rotrdi	%r4,%r3,63

	rotld	%r5,%r3,%r4

	sldi	%r4,%r3,0
	sldi	%r4,%r3,63

	srdi	%r4,%r3,0
	srdi	%r4,%r3,1
	srdi	%r4,%r3,63

	clrldi	%r4,%r3,0
	clrldi	%r4,%r3,1
	clrldi	%r4,%r3,63

	clrrdi	%r4,%r3,0
	clrrdi	%r4,%r3,1
	clrrdi	%r4,%r3,63
	
	clrlsldi	%r4,%r3,0,0
	clrlsldi	%r4,%r3,1,0
	clrlsldi	%r4,%r3,63,0
	clrlsldi	%r4,%r3,63,1
	clrlsldi	%r4,%r3,63,63
	
# These test the remaining corner cases for 32-bit operations.
	extlwi	%r4,%r3,1,0
	extlwi	%r4,%r3,32,0
	extlwi	%r4,%r3,1,31
	extlwi	%r4,%r3,32,31    # bit weird, that one.
	
	extrwi	%r4,%r3,31,0
	extrwi	%r4,%r3,1,30
	
	inslwi	%r4,%r3,1,0
	inslwi	%r4,%r3,32,0
	inslwi	%r4,%r3,1,31
	
	insrwi	%r4,%r3,1,0
	insrwi	%r4,%r3,32,0
	insrwi	%r4,%r3,1,31
	
	rotlwi	%r4,%r3,0
	rotlwi	%r4,%r3,1
	rotlwi	%r4,%r3,31

	rotrwi	%r4,%r3,0
	rotrwi	%r4,%r3,1
	rotrwi	%r4,%r3,31

	rotlw	%r5,%r3,%r4

	slwi	%r4,%r3,0
	slwi	%r4,%r3,1
	slwi	%r4,%r3,31

	srwi	%r4,%r3,0
	srwi	%r4,%r3,1
	srwi	%r4,%r3,31

	clrlwi	%r4,%r3,0
	clrlwi	%r4,%r3,1
	clrlwi	%r4,%r3,31

	clrrwi	%r4,%r3,0
	clrrwi	%r4,%r3,1
	clrrwi	%r4,%r3,31
	
	clrlslwi	%r4,%r3,0,0
	clrlslwi	%r4,%r3,1,0
	clrlslwi	%r4,%r3,31,0
	clrlslwi	%r4,%r3,31,1
	clrlslwi	%r4,%r3,31,31

# Force alignment so that we pass the test on AIX
	.p2align	3,0
