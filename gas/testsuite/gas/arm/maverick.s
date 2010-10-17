	.text
	.align
load_store:
	cfldrseq	mvf5, [sp, #252]
	cfldrsmi	mvf14, [r11, #72]
	cfldrsvc	mvf2, [r12, #-240]
	cfldrslt	mvf0, [sl, #252]
	cfldrsgt	mvf10, [fp, #72]
	cfldrsle	mvf6, [ip, #-240]!
	cfldrsls	mvf0, [r10, #252]!
	cfldrsmi	mvf14, [r11, #72]!
	cfldrsvc	mvf2, [r12, #-240]!
	cfldrslt	mvf0, [sl, #252]!
	cfldrsgt	mvf10, [fp], #72
	cfldrsle	mvf6, [ip], #-240
	cfldrsls	mvf0, [r10], #252
	cfldrsmi	mvf14, [r11], #72
	cfldrsvc	mvf2, [r12], #-240
	cfldrdlt	mvd0, [sl, #252]
	cfldrdgt	mvd10, [fp, #72]
	cfldrdle	mvd6, [ip, #-240]
	cfldrdls	mvd0, [r10, #252]
	cfldrdmi	mvd14, [r11, #72]
	cfldrdvc	mvd2, [r12, #-240]!
	cfldrdlt	mvd0, [sl, #252]!
	cfldrdgt	mvd10, [fp, #72]!
	cfldrdle	mvd6, [ip, #-240]!
	cfldrdls	mvd0, [r10, #252]!
	cfldrdmi	mvd14, [r11], #72
	cfldrdvc	mvd2, [r12], #-240
	cfldrdlt	mvd0, [sl], #252
	cfldrdgt	mvd10, [fp], #72
	cfldrdle	mvd6, [ip], #-240
	cfldr32ls	mvfx0, [r10, #252]
	cfldr32mi	mvfx14, [r11, #72]
	cfldr32vc	mvfx2, [r12, #-240]
	cfldr32lt	mvfx0, [sl, #252]
	cfldr32gt	mvfx10, [fp, #72]
	cfldr32le	mvfx6, [ip, #-240]!
	cfldr32ls	mvfx0, [r10, #252]!
	cfldr32mi	mvfx14, [r11, #72]!
	cfldr32vc	mvfx2, [r12, #-240]!
	cfldr32lt	mvfx0, [sl, #252]!
	cfldr32gt	mvfx10, [fp], #72
	cfldr32le	mvfx6, [ip], #-240
	cfldr32ls	mvfx0, [r10], #252
	cfldr32mi	mvfx14, [r11], #72
	cfldr32vc	mvfx2, [r12], #-240
	cfldr64lt	mvdx0, [sl, #252]
	cfldr64gt	mvdx10, [fp, #72]
	cfldr64le	mvdx6, [ip, #-240]
	cfldr64ls	mvdx0, [r10, #252]
	cfldr64mi	mvdx14, [r11, #72]
	cfldr64vc	mvdx2, [r12, #-240]!
	cfldr64lt	mvdx0, [sl, #252]!
	cfldr64gt	mvdx10, [fp, #72]!
	cfldr64le	mvdx6, [ip, #-240]!
	cfldr64ls	mvdx0, [r10, #252]!
	cfldr64mi	mvdx14, [r11], #72
	cfldr64vc	mvdx2, [r12], #-240
	cfldr64lt	mvdx0, [sl], #252
	cfldr64gt	mvdx10, [fp], #72
	cfldr64le	mvdx6, [ip], #-240
	cfstrsls	mvf0, [r10, #252]
	cfstrsmi	mvf14, [r11, #72]
	cfstrsvc	mvf2, [r12, #-240]
	cfstrslt	mvf0, [sl, #252]
	cfstrsgt	mvf10, [fp, #72]
	cfstrsle	mvf6, [ip, #-240]!
	cfstrsls	mvf0, [r10, #252]!
	cfstrsmi	mvf14, [r11, #72]!
	cfstrsvc	mvf2, [r12, #-240]!
	cfstrslt	mvf0, [sl, #252]!
	cfstrsgt	mvf10, [fp], #72
	cfstrsle	mvf6, [ip], #-240
	cfstrsls	mvf0, [r10], #252
	cfstrsmi	mvf14, [r11], #72
	cfstrsvc	mvf2, [r12], #-240
	cfstrdlt	mvd0, [sl, #252]
	cfstrdgt	mvd10, [fp, #72]
	cfstrdle	mvd6, [ip, #-240]
	cfstrdls	mvd0, [r10, #252]
	cfstrdmi	mvd14, [r11, #72]
	cfstrdvc	mvd2, [r12, #-240]!
	cfstrdlt	mvd0, [sl, #252]!
	cfstrdgt	mvd10, [fp, #72]!
	cfstrdle	mvd6, [ip, #-240]!
	cfstrdls	mvd0, [r10, #252]!
	cfstrdmi	mvd14, [r11], #72
	cfstrdvc	mvd2, [r12], #-240
	cfstrdlt	mvd0, [sl], #252
	cfstrdgt	mvd10, [fp], #72
	cfstrdle	mvd6, [ip], #-240
	cfstr32ls	mvfx0, [r10, #252]
	cfstr32mi	mvfx14, [r11, #72]
	cfstr32vc	mvfx2, [r12, #-240]
	cfstr32lt	mvfx0, [sl, #252]
	cfstr32gt	mvfx10, [fp, #72]
	cfstr32le	mvfx6, [ip, #-240]!
	cfstr32ls	mvfx0, [r10, #252]!
	cfstr32mi	mvfx14, [r11, #72]!
	cfstr32vc	mvfx2, [r12, #-240]!
	cfstr32lt	mvfx0, [sl, #252]!
	cfstr32gt	mvfx10, [fp], #72
	cfstr32le	mvfx6, [ip], #-240
	cfstr32ls	mvfx0, [r10], #252
	cfstr32mi	mvfx14, [r11], #72
	cfstr32vc	mvfx2, [r12], #-240
	cfstr64lt	mvdx0, [sl, #252]
	cfstr64gt	mvdx10, [fp, #72]
	cfstr64le	mvdx6, [ip, #-240]
	cfstr64ls	mvdx0, [r10, #252]
	cfstr64mi	mvdx14, [r11, #72]
	cfstr64vc	mvdx2, [r12, #-240]!
	cfstr64lt	mvdx0, [sl, #252]!
	cfstr64gt	mvdx10, [fp, #72]!
	cfstr64le	mvdx6, [ip, #-240]!
	cfstr64ls	mvdx0, [r10, #252]!
	cfstr64mi	mvdx14, [r11], #72
	cfstr64vc	mvdx2, [r12], #-240
	cfstr64lt	mvdx0, [sl], #252
	cfstr64gt	mvdx10, [fp], #72
	cfstr64le	mvdx6, [ip], #-240
move:
	cfmvsrls	mvf0, r10
	cfmvsr	mvf10, r4
	cfmvsrmi	mvf14, r11
	cfmvsrhi	mvf13, r5
	cfmvsrcs	mvf1, r6
	cfmvrsvs	r3, mvf0
	cfmvrsvc	r13, mvf14
	cfmvrscc	r14, mvf10
	cfmvrsne	r8, mvf15
	cfmvrsle	r15, mvf11
	cfmvdlrmi	mvd2, r3
	cfmvdlreq	mvd5, sp
	cfmvdlrge	mvd9, lr
	cfmvdlral	mvd3, r8
	cfmvdlrle	mvd7, pc
	cfmvrdlne	r6, mvd6
	cfmvrdllt	r0, mvd7
	cfmvrdlpl	r7, mvd3
	cfmvrdlgt	r1, mvd1
	cfmvrdlhi	r2, mvd13
	cfmvdhrvs	mvd11, r6
	cfmvdhrcs	mvd9, r0
	cfmvdhrpl	mvd15, r7
	cfmvdhrls	mvd4, r1
	cfmvdhrcc	mvd8, r2
	cfmvrdhvc	pc, mvd1
	cfmvrdhgt	r9, mvd11
	cfmvrdheq	sl, mvd5
	cfmvrdhal	r4, mvd12
	cfmvrdhge	fp, mvd8
	cfmv64lr	mvdx13, r15
	cfmv64lrlt	mvdx4, r9
	cfmv64lrls	mvdx0, r10
	cfmv64lr	mvdx10, r4
	cfmv64lrmi	mvdx14, r11
	cfmvr64lhi	r2, mvdx7
	cfmvr64lcs	r12, mvdx12
	cfmvr64lvs	r3, mvdx0
	cfmvr64lvc	r13, mvdx14
	cfmvr64lcc	r14, mvdx10
	cfmv64hrne	mvdx8, r2
	cfmv64hrle	mvdx6, ip
	cfmv64hrmi	mvdx2, r3
	cfmv64hreq	mvdx5, sp
	cfmv64hrge	mvdx9, lr
	cfmvr64hal	r11, mvdx8
	cfmvr64hle	r5, mvdx2
	cfmvr64hne	r6, mvdx6
	cfmvr64hlt	r0, mvdx7
	cfmvr64hpl	r7, mvdx3
	cfmval32gt	mvax1, mvfx1
	cfmval32hi	mvax3, mvfx13
	cfmval32vs	mvax3, mvfx4
	cfmval32cs	mvax1, mvfx0
	cfmval32pl	mvax3, mvfx10
	cfmv32alls	mvfx4, mvax1
	cfmv32alcc	mvfx8, mvax3
	cfmv32alvc	mvfx2, mvax3
	cfmv32algt	mvfx6, mvax1
	cfmv32aleq	mvfx7, mvax3
	cfmvam32al	mvax2, mvfx12
	cfmvam32ge	mvax3, mvfx8
	cfmvam32	mvax2, mvfx6
	cfmvam32lt	mvax2, mvfx2
	cfmvam32ls	mvax0, mvfx5
	cfmv32am	mvfx10, mvax2
	cfmv32ammi	mvfx14, mvax3
	cfmv32amhi	mvfx13, mvax2
	cfmv32amcs	mvfx1, mvax2
	cfmv32amvs	mvfx11, mvax0
	cfmvah32vc	mvax3, mvfx14
	cfmvah32cc	mvax0, mvfx10
	cfmvah32ne	mvax1, mvfx15
	cfmvah32le	mvax0, mvfx11
	cfmvah32mi	mvax0, mvfx9
	cfmv32aheq	mvfx5, mvax3
	cfmv32ahge	mvfx9, mvax0
	cfmv32ahal	mvfx3, mvax1
	cfmv32ahle	mvfx7, mvax0
	cfmv32ahne	mvfx12, mvax0
	cfmva32lt	mvax0, mvfx7
	cfmva32pl	mvax2, mvfx3
	cfmva32gt	mvax1, mvfx1
	cfmva32hi	mvax3, mvfx13
	cfmva32vs	mvax3, mvfx4
	cfmv32acs	mvfx9, mvax0
	cfmv32apl	mvfx15, mvax2
	cfmv32als	mvfx4, mvax1
	cfmv32acc	mvfx8, mvax3
	cfmv32avc	mvfx2, mvax3
	cfmva64gt	mvax0, mvdx11
	cfmva64eq	mvax1, mvdx5
	cfmva64al	mvax2, mvdx12
	cfmva64ge	mvax3, mvdx8
	cfmva64	mvax2, mvdx6
	cfmv64alt	mvdx4, mvax0
	cfmv64als	mvdx0, mvax1
	cfmv64a	mvdx10, mvax2
	cfmv64ami	mvdx14, mvax3
	cfmv64ahi	mvdx13, mvax2
	cfmvsc32cs	dspsc, mvdx12
	cfmvsc32vs	dspsc, mvdx0
	cfmvsc32vc	dspsc, mvdx14
	cfmvsc32cc	dspsc, mvdx10
	cfmvsc32ne	dspsc, mvdx15
	cfmv32scle	mvdx6, dspsc
	cfmv32scmi	mvdx2, dspsc
	cfmv32sceq	mvdx5, dspsc
	cfmv32scge	mvdx9, dspsc
	cfmv32scal	mvdx3, dspsc
	cfcpysle	mvf7, mvf2
	cfcpysne	mvf12, mvf6
	cfcpyslt	mvf0, mvf7
	cfcpyspl	mvf14, mvf3
	cfcpysgt	mvf10, mvf1
	cfcpydhi	mvd15, mvd13
	cfcpydvs	mvd11, mvd4
	cfcpydcs	mvd9, mvd0
	cfcpydpl	mvd15, mvd10
	cfcpydls	mvd4, mvd14
conv:
	cfcvtsdcc	mvd8, mvf13
	cfcvtsdvc	mvd2, mvf1
	cfcvtsdgt	mvd6, mvf11
	cfcvtsdeq	mvd7, mvf5
	cfcvtsdal	mvd3, mvf12
	cfcvtdsge	mvf1, mvd8
	cfcvtds	mvf13, mvd6
	cfcvtdslt	mvf4, mvd2
	cfcvtdsls	mvf0, mvd5
	cfcvtds	mvf10, mvd9
	cfcvt32smi	mvf14, mvfx3
	cfcvt32shi	mvf13, mvfx7
	cfcvt32scs	mvf1, mvfx12
	cfcvt32svs	mvf11, mvfx0
	cfcvt32svc	mvf5, mvfx14
	cfcvt32dcc	mvd12, mvfx10
	cfcvt32dne	mvd8, mvfx15
	cfcvt32dle	mvd6, mvfx11
	cfcvt32dmi	mvd2, mvfx9
	cfcvt32deq	mvd5, mvfx15
	cfcvt64sge	mvf9, mvdx4
	cfcvt64sal	mvf3, mvdx8
	cfcvt64sle	mvf7, mvdx2
	cfcvt64sne	mvf12, mvdx6
	cfcvt64slt	mvf0, mvdx7
	cfcvt64dpl	mvd14, mvdx3
	cfcvt64dgt	mvd10, mvdx1
	cfcvt64dhi	mvd15, mvdx13
	cfcvt64dvs	mvd11, mvdx4
	cfcvt64dcs	mvd9, mvdx0
	cfcvts32pl	mvfx15, mvf10
	cfcvts32ls	mvfx4, mvf14
	cfcvts32cc	mvfx8, mvf13
	cfcvts32vc	mvfx2, mvf1
	cfcvts32gt	mvfx6, mvf11
	cfcvtd32eq	mvfx7, mvd5
	cfcvtd32al	mvfx3, mvd12
	cfcvtd32ge	mvfx1, mvd8
	cfcvtd32	mvfx13, mvd6
	cfcvtd32lt	mvfx4, mvd2
	cftruncs32ls	mvfx0, mvf5
	cftruncs32	mvfx10, mvf9
	cftruncs32mi	mvfx14, mvf3
	cftruncs32hi	mvfx13, mvf7
	cftruncs32cs	mvfx1, mvf12
	cftruncd32vs	mvfx11, mvd0
	cftruncd32vc	mvfx5, mvd14
	cftruncd32cc	mvfx12, mvd10
	cftruncd32ne	mvfx8, mvd15
	cftruncd32le	mvfx6, mvd11
shift:
	cfrshl32mi	mvfx2, mvfx9, r0
	cfrshl32	mvfx10, mvfx9, lr
	cfrshl32cc	mvfx8, mvfx13, r5
	cfrshl32ne	mvfx12, mvfx6, r3
	cfrshl32vc	mvfx5, mvfx14, r4
	cfrshl64ge	mvdx1, mvdx8, r2
	cfrshl64vs	mvdx11, mvdx4, r9
	cfrshl64eq	mvdx5, mvdx15, r7
	cfrshl64mi	mvdx14, mvdx3, r8
	cfrshl64vc	mvdx2, mvdx1, r6
	cfsh32lt	mvfx0, mvfx7, #-64
	cfsh32cc	mvfx12, mvfx10, #-20
	cfsh32	mvfx13, mvfx6, #40
	cfsh32cs	mvfx9, mvfx0, #-1
	cfsh32ge	mvfx9, mvfx4, #24
	cfsh64hi	mvdx13, mvdx7, #33
	cfsh64gt	mvdx6, mvdx11, #0
	cfsh64pl	mvdx14, mvdx3, #32
	cfsh64ne	mvdx8, mvdx15, #-31
	cfsh64lt	mvdx4, mvdx2, #1
comp:
	cfcmpspl	sp, mvf10, mvf9
	cfcmpsal	r11, mvf8, mvf13
	cfcmpscs	r12, mvf12, mvf6
	cfcmpseq	sl, mvf5, mvf14
	cfcmpsgt	r1, mvf1, mvf8
	cfcmpdle	r15, mvd11, mvd4
	cfcmpdls	r0, mvd5, mvd15
	cfcmpdls	lr, mvd14, mvd3
	cfcmpdle	r5, mvd2, mvd1
	cfcmpdvs	r3, mvd0, mvd7
	cfcmp32al	r4, mvfx12, mvfx10
	cfcmp32hi	r2, mvfx13, mvfx6
	cfcmp32mi	r9, mvfx9, mvfx0
	cfcmp32	r7, mvfx9, mvfx4
	cfcmp32cc	r8, mvfx13, mvfx7
	cfcmp64ne	r6, mvdx6, mvdx11
	cfcmp64vc	r13, mvdx14, mvdx3
	cfcmp64ge	fp, mvdx8, mvdx15
	cfcmp64vs	ip, mvdx4, mvdx2
	cfcmp64eq	r10, mvdx15, mvdx10
fp_arith:
	cfabssmi	mvf14, mvf3
	cfabsshi	mvf13, mvf7
	cfabsscs	mvf1, mvf12
	cfabssvs	mvf11, mvf0
	cfabssvc	mvf5, mvf14
	cfabsdcc	mvd12, mvd10
	cfabsdne	mvd8, mvd15
	cfabsdle	mvd6, mvd11
	cfabsdmi	mvd2, mvd9
	cfabsdeq	mvd5, mvd15
	cfnegsge	mvf9, mvf4
	cfnegsal	mvf3, mvf8
	cfnegsle	mvf7, mvf2
	cfnegsne	mvf12, mvf6
	cfnegslt	mvf0, mvf7
	cfnegdpl	mvd14, mvd3
	cfnegdgt	mvd10, mvd1
	cfnegdhi	mvd15, mvd13
	cfnegdvs	mvd11, mvd4
	cfnegdcs	mvd9, mvd0
	cfaddspl	mvf15, mvf10, mvf9
	cfaddsal	mvf3, mvf8, mvf13
	cfaddscs	mvf1, mvf12, mvf6
	cfaddseq	mvf7, mvf5, mvf14
	cfaddsgt	mvf10, mvf1, mvf8
	cfadddle	mvd6, mvd11, mvd4
	cfadddls	mvd0, mvd5, mvd15
	cfadddls	mvd4, mvd14, mvd3
	cfadddle	mvd7, mvd2, mvd1
	cfadddvs	mvd11, mvd0, mvd7
	cfsubsal	mvf3, mvf12, mvf10
	cfsubshi	mvf15, mvf13, mvf6
	cfsubsmi	mvf2, mvf9, mvf0
	cfsubs	mvf10, mvf9, mvf4
	cfsubscc	mvf8, mvf13, mvf7
	cfsubdne	mvd12, mvd6, mvd11
	cfsubdvc	mvd5, mvd14, mvd3
	cfsubdge	mvd1, mvd8, mvd15
	cfsubdvs	mvd11, mvd4, mvd2
	cfsubdeq	mvd5, mvd15, mvd10
	cfmulsmi	mvf14, mvf3, mvf8
	cfmulsvc	mvf2, mvf1, mvf12
	cfmulslt	mvf0, mvf7, mvf5
	cfmulscc	mvf12, mvf10, mvf1
	cfmuls	mvf13, mvf6, mvf11
	cfmuldcs	mvd9, mvd0, mvd5
	cfmuldge	mvd9, mvd4, mvd14
	cfmuldhi	mvd13, mvd7, mvd2
	cfmuldgt	mvd6, mvd11, mvd0
	cfmuldpl	mvd14, mvd3, mvd12
int_arith:
	cfabs32ne	mvfx8, mvfx15
	cfabs32le	mvfx6, mvfx11
	cfabs32mi	mvfx2, mvfx9
	cfabs32eq	mvfx5, mvfx15
	cfabs32ge	mvfx9, mvfx4
	cfabs64al	mvdx3, mvdx8
	cfabs64le	mvdx7, mvdx2
	cfabs64ne	mvdx12, mvdx6
	cfabs64lt	mvdx0, mvdx7
	cfabs64pl	mvdx14, mvdx3
	cfneg32gt	mvfx10, mvfx1
	cfneg32hi	mvfx15, mvfx13
	cfneg32vs	mvfx11, mvfx4
	cfneg32cs	mvfx9, mvfx0
	cfneg32pl	mvfx15, mvfx10
	cfneg64ls	mvdx4, mvdx14
	cfneg64cc	mvdx8, mvdx13
	cfneg64vc	mvdx2, mvdx1
	cfneg64gt	mvdx6, mvdx11
	cfneg64eq	mvdx7, mvdx5
	cfadd32al	mvfx3, mvfx12, mvfx10
	cfadd32hi	mvfx15, mvfx13, mvfx6
	cfadd32mi	mvfx2, mvfx9, mvfx0
	cfadd32	mvfx10, mvfx9, mvfx4
	cfadd32cc	mvfx8, mvfx13, mvfx7
	cfadd64ne	mvdx12, mvdx6, mvdx11
	cfadd64vc	mvdx5, mvdx14, mvdx3
	cfadd64ge	mvdx1, mvdx8, mvdx15
	cfadd64vs	mvdx11, mvdx4, mvdx2
	cfadd64eq	mvdx5, mvdx15, mvdx10
	cfsub32mi	mvfx14, mvfx3, mvfx8
	cfsub32vc	mvfx2, mvfx1, mvfx12
	cfsub32lt	mvfx0, mvfx7, mvfx5
	cfsub32cc	mvfx12, mvfx10, mvfx1
	cfsub32	mvfx13, mvfx6, mvfx11
	cfsub64cs	mvdx9, mvdx0, mvdx5
	cfsub64ge	mvdx9, mvdx4, mvdx14
	cfsub64hi	mvdx13, mvdx7, mvdx2
	cfsub64gt	mvdx6, mvdx11, mvdx0
	cfsub64pl	mvdx14, mvdx3, mvdx12
	cfmul32ne	mvfx8, mvfx15, mvfx13
	cfmul32lt	mvfx4, mvfx2, mvfx9
	cfmul32pl	mvfx15, mvfx10, mvfx9
	cfmul32al	mvfx3, mvfx8, mvfx13
	cfmul32cs	mvfx1, mvfx12, mvfx6
	cfmul64eq	mvdx7, mvdx5, mvdx14
	cfmul64gt	mvdx10, mvdx1, mvdx8
	cfmul64le	mvdx6, mvdx11, mvdx4
	cfmul64ls	mvdx0, mvdx5, mvdx15
	cfmul64ls	mvdx4, mvdx14, mvdx3
	cfmac32le	mvfx7, mvfx2, mvfx1
	cfmac32vs	mvfx11, mvfx0, mvfx7
	cfmac32al	mvfx3, mvfx12, mvfx10
	cfmac32hi	mvfx15, mvfx13, mvfx6
	cfmac32mi	mvfx2, mvfx9, mvfx0
	cfmsc32	mvfx10, mvfx9, mvfx4
	cfmsc32cc	mvfx8, mvfx13, mvfx7
	cfmsc32ne	mvfx12, mvfx6, mvfx11
	cfmsc32vc	mvfx5, mvfx14, mvfx3
	cfmsc32ge	mvfx1, mvfx8, mvfx15
acc_arith:
	cfmadd32vs	mvax3, mvfx4, mvfx2, mvfx9
	cfmadd32eq	mvax1, mvfx15, mvfx10, mvfx9
	cfmadd32mi	mvax1, mvfx3, mvfx8, mvfx13
	cfmadd32vc	mvax0, mvfx1, mvfx12, mvfx6
	cfmadd32lt	mvax0, mvfx7, mvfx5, mvfx14
	cfmsub32cc	mvax0, mvfx10, mvfx1, mvfx8
	cfmsub32	mvax2, mvfx6, mvfx11, mvfx4
	cfmsub32cs	mvax1, mvfx0, mvfx5, mvfx15
	cfmsub32ge	mvax2, mvfx4, mvfx14, mvfx3
	cfmsub32hi	mvax3, mvfx7, mvfx2, mvfx1
	cfmadda32gt	mvax0, mvax1, mvfx0, mvfx7
	cfmadda32pl	mvax2, mvax2, mvfx12, mvfx10
	cfmadda32ne	mvax1, mvax3, mvfx13, mvfx6
	cfmadda32lt	mvax2, mvax0, mvfx9, mvfx0
	cfmadda32pl	mvax3, mvax2, mvfx9, mvfx4
	cfmsuba32al	mvax3, mvax1, mvfx13, mvfx7
	cfmsuba32cs	mvax3, mvax2, mvfx6, mvfx11
	cfmsuba32eq	mvax1, mvax3, mvfx14, mvfx3
	cfmsuba32gt	mvax1, mvax3, mvfx8, mvfx15
	cfmsuba32le	mvax0, mvax3, mvfx4, mvfx2
