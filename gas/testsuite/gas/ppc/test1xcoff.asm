dnl divert(-1)
ifdef(`XCOFF64',
`	define(`WORD',`.llong')
	define(`LDW',`ld')')
ifdef(`XCOFF32',
`	define(`WORD',`.long')
	define(`LDW',`lwz')')
dnl divert(0) dnl

define(`nl',`
') nl nl nl nl nl nl

	.csect	[RW]
dsym0:	WORD	0xdeadbeef
dsym1:

	.toc
.L_tsym0:
	.tc	ignored0[TC],dsym0
.L_tsym1:
	.tc	ignored1[TC],dsym1
.L_tsym2:
	.tc	ignored2[TC],usym0
.L_tsym3:
	.tc	ignored3[TC],usym1
.L_tsym4:
	.tc	ignored4[TC],esym0
.L_tsym5:
	.tc	ignored5[TC],esym1
.L_tsym6:
	.tc	ignored6[TC],.text

	.csect	.crazy_table[RO]
xdsym0:	WORD	0xbeefed
xdsym1:
	.csect	[PR]
	.lglobl	reference_csect_relative_symbols
reference_csect_relative_symbols:
	LDW	3,xdsym0(3)
	LDW	3,xdsym1(3)
	LDW	3,xusym0(3)
	LDW	3,xusym1(3)

	.lglobl	dubious_references_to_default_RW_csect
dubious_references_to_default_RW_csect:
	LDW	3,dsym0(3)
	LDW	3,dsym1(3)
	LDW	3,usym0(3)
	LDW	3,usym1(3)

	.lglobl	reference_via_toc
reference_via_toc:
	LDW	3,.L_tsym0(2)
	LDW	3,.L_tsym1(2)
	LDW	3,.L_tsym2(2)
	LDW	3,.L_tsym3(2)
	LDW	3,.L_tsym4(2)
	LDW	3,.L_tsym5(2)

	.lglobl	subtract_symbols
subtract_symbols:
	li	3,dsym1-dsym0
	li	3,dsym0-dsym1
	li	3,usym1-usym0
	li	3,usym0-usym1
	li	3,dsym0-usym0
	li	3,usym0-dsym0
	LDW	3,dsym1-dsym0(4)

	.lglobl	load_addresses
load_addresses:
	la	3,xdsym0(0)
	la	3,xusym0(0)

	la	3,.L_tsym6(2)

	.csect	[RW]
usym0:	WORD	0xcafebabe
usym1:  WORD    0xbaad
	.csect	.crazy_table[RO]
xusym0:	WORD	0xbeefed
xusym1:
