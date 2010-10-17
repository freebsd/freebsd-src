.text
foo:
	.insn e,0x0101
	.insn ri,0xa70a0000,%r1,-32767
	.insn rr,0x1800,%r1,%r2
	.insn rre,0xb25e0000,%r1,%r2
	.insn rrf,0xb35b0000,%f1,%f2,9,%f3
	.insn rs,0xba000000,%r1,%r2,3(%r4)
	.insn rsi,0x84000000,%r1,%r2,test_rsi
	.insn rx,0x58000000,%r1,2(%r3,%r4)
	.insn rxe,0xed000000001a,%f1,2(%r3)
	.insn rxf,0xed000000001e,%f1,%f2,3(%r4,%r5)
	.insn s,0xb2330000,1(%r2)
	.insn si,0x92000000,1(%r2),3
	.insn ss,0xd20000000000,1(2,%r3),4(%r5),6
	.insn sse,0xe50100000000,1(%r2),3(%r4)
