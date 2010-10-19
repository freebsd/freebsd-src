.text
foo:
	.insn rie,0xec0000000045,%r1,%r2,test_rie
	.insn ril,0xc00500000000,%r14,test_ril
	.insn rse,0xeb000000000d,%r1,%r2,3(%r4)
	.insn ssf,0xc80000000000,1(%r2),3(%r4),%r5
