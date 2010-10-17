dnl divert(-1)
ifdef(`ELF64',
`	define(`WORD',`.llong')
	define(`LDW',`ld')')
ifdef(`ELF32',
`	define(`WORD',`.long')
	define(`LDW',`lwz')')
dnl divert(0) dnl

define(`nl',`
') nl nl nl nl nl nl

	.section	".data"
dsym0:	WORD	0xdeadbeef
dsym1:

ifdef(`ELF64',`
	.section	".toc"
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
')

	.section	".text"
	LDW	3,dsym0@l(3)
	LDW	3,dsym1@l(3)
	LDW	3,usym0@l(3)
	LDW	3,usym1@l(3)
	LDW	3,esym0@l(3)
	LDW	3,esym1@l(3)

ifdef(`ELF64',`
	LDW	3,.L_tsym0@toc(2)
	LDW	3,.L_tsym1@toc(2)
	LDW	3,.L_tsym2@toc(2)
	LDW	3,.L_tsym3@toc(2)
	LDW	3,.L_tsym4@toc(2)
	LDW	3,.L_tsym5@toc(2)

	lis	4,.L_tsym5@toc@ha
	LDW	3,.L_tsym5@toc@l(2)
')

	li	3,dsym1-dsym0
	li	3,dsym0-dsym1
	li	3,usym1-usym0
	li	3,usym0-usym1
	li	3,dsym0-usym0
	li	3,usym0-dsym0

	li	3,dsym0@l
	li	3,dsym0@h
	li	3,dsym0@ha
ifdef(`ELF64',`
	li	3,dsym0@higher
	li	3,dsym0@highera
	li	3,dsym0@highest
	li	3,dsym0@highesta
')

	li	3,usym0-usym1@l
	li	3,usym0-usym1@h
	li	3,usym0-usym1@ha
ifdef(`ELF64',`
	li	3,usym0-usym1@higher
	li	3,usym0-usym1@highera
	li	3,usym0-usym1@highest
	li	3,usym0-usym1@highesta
')

	LDW	3,dsym1-dsym0@l(4)

	LDW	3,.text@l(0)

	.section	".data"
usym0:	WORD	0xcafebabe
usym1:

datpt:	.long	jk-.+10000000
dat0:	.long	jk-dat1
dat1:	.long	jk-dat1
dat2:	.long	jk-dat1
ifdef(`ELF64',`
dat3:	.llong	jk-dat1
dat4:	.llong	jk-dat1
')
