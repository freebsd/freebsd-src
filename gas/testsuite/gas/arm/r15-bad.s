.text
.align 0

label:
	mul r15, r1, r2
	mul r1, r15, r2
	mla r15, r2, r3, r4
	mla r1, r15, r3, r4
	mla r1, r2, r15, r4
	mla r1, r2, r3, r15
	smlabb r15, r2, r3, r4
	smlabb r1, r15, r3, r4
	smlabb r1, r2, r15, r4
	smlabb r1, r2, r3, r15
	smlalbb r15, r2, r3, r4
	smlalbb r1, r15, r3, r4
	smlalbb r1, r2, r15, r4
	smlalbb r1, r2, r3, r15
	smulbb r15, r2, r3
	smulbb r1, r15, r3
	smulbb r1, r2, r15
	qadd r15, r2, r3
	qadd r1, r15, r3
	qadd r1, r2, r15
	qadd16 r15, r2, r3
	qadd16 r1, r15, r3
	qadd16 r1, r2, r15
	clz r15, r2
	clz r1, r15
	umaal r15, r2, r3, r4
	umaal r1, r15, r3, r4
	umaal r1, r2, r15, r4
	umaal r1, r2, r3, r15
	strex r15, r2, [r3]
	strex r1, r15, [r3]
	strex r1, r2, [r15]
	ssat r15, #1, r2
	ssat r1, #1, r15
	ssat16 r15, #1, r2
	ssat16 r1, #1, r15
	smmul r15, r2, r3
	smmul r1, r15, r3
	smmul r1, r2, r15
	smlald r15, r2, r3, r4
	smlald r1, r15, r3, r4
	smlald r1, r2, r15, r4
	smlald r1, r2, r3, r15
	smlad r15, r2, r3, r4
	smlad r1, r15, r3, r4
	smlad r1, r2, r15, r4
	smlad r1, r2, r3, r15
	sxth r15, r2
	sxth r1, r15
	sxtah r15, r2, r3
	sxtah r1, r15, r3
	sxtah r1, r2, r15
	rfeda r15
	rev r15, r2
	rev r1, r15
	pkhtb r15, r2, r3
	pkhtb r1, r15, r3
	pkhtb r1, r2, r15
	ldrex r15, [r2]
	ldrex r1, [r15]
	swp r15, r2, [r3]
	swp r1, r15, [r3]
	swp r1, r2, [r15]

