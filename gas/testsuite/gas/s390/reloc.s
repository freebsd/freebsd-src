	.text
	.globl foo
foo:
	mvc	0(test_R_390_8,%r1),0(%r2)
	l	%r0,test_R_390_12(%r1,%r2)
	lhi	%r0,test_R_390_16
	.long	test_R_390_32
	.long	test_R_390_PC32-foo
	l	%r0,test_R_390_GOT12@GOT(%r1,%r2)
	.long	test_R_390_GOT32@GOT
	.long	test_R_390_PLT32@PLT
	lhi	%r0,test_R_390_GOT16@GOT
	lhi	%r0,test_R_390_PC16-foo
	bras	%r14,test_R_390_PC16DBL
	bras	%r14,test_R_390_PLT16DBL
	lhi	%r0,test_R_390_GOTOFF16@GOTOFF
	.long	test_R_390_GOTOFF32@GOTOFF
	lhi	%r0,test_R_390_PLTOFF16@PLTOFF
	.long	test_R_390_PLTOFF32@PLTOFF
	l	%r0,test_R_390_GOTPLT12@GOTPLT(%r1,%r2)
	lhi	%r0,test_R_390_GOTPLT16@GOTPLT
	.long	test_R_390_GOTPLT32@GOTPLT

bar:
	bras	%r14,foo@PLT
	.long	foo@PLT-bar
