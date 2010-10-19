	.text
	.align
load_store:
	cfldrseq	mvf5, [sp, #1020]
	cfldrsmi	mvf14, [r11, #292]
	cfldrsvc	mvf2, [r12, #-956]
	cfldrslt	mvf0, [sl, #-1020]
	cfldrscc	mvf12, [r1, #-156]
	cfldrs	mvf13, [r9, #416]!
	cfldrscs	mvf9, [r0, #-1020]!
	cfldrsls	mvf4, [r1, #-156]!
	cfldrsle	mvf7, [r9, #416]!
	cfldrsvs	mvf11, [r0, #-1020]!
	cfldrscc	mvf12, [r1], #-156
	cfldrs	mvf13, [r9], #416
	cfldrscs	mvf9, [r0], #-1020
	cfldrsls	mvf4, [r1], #-156
	cfldrsle	mvf7, [r9], #416
	cfldrdvs	mvd11, [r0, #-1020]
	cfldrdcc	mvd12, [r1, #-156]
	cfldrd	mvd13, [r9, #416]
	cfldrdcs	mvd9, [r0, #-1020]
	cfldrdls	mvd4, [r1, #-156]
	cfldrdle	mvd7, [r9, #416]!
	cfldrdvs	mvd11, [r0, #-1020]!
	cfldrdcc	mvd12, [r1, #-156]!
	cfldrd	mvd13, [r9, #416]!
	cfldrdcs	mvd9, [r0, #-1020]!
	cfldrdls	mvd4, [r1], #-156
	cfldrdle	mvd7, [r9], #416
	cfldrdvs	mvd11, [r0], #-1020
	cfldrdcc	mvd12, [r1], #-156
	cfldrd	mvd13, [r9], #416
	cfldr32cs	mvfx9, [r0, #-1020]
	cfldr32ls	mvfx4, [r1, #-156]
	cfldr32le	mvfx7, [r9, #416]
	cfldr32vs	mvfx11, [r0, #-1020]
	cfldr32cc	mvfx12, [r1, #-156]
	cfldr32	mvfx13, [r9, #416]!
	cfldr32cs	mvfx9, [r0, #-1020]!
	cfldr32ls	mvfx4, [r1, #-156]!
	cfldr32le	mvfx7, [r9, #416]!
	cfldr32vs	mvfx11, [r0, #-1020]!
	cfldr32cc	mvfx12, [r1], #-156
	cfldr32	mvfx13, [r9], #416
	cfldr32cs	mvfx9, [r0], #-1020
	cfldr32ls	mvfx4, [r1], #-156
	cfldr32le	mvfx7, [r9], #416
	cfldr64vs	mvdx11, [r0, #-1020]
	cfldr64cc	mvdx12, [r1, #-156]
	cfldr64	mvdx13, [r9, #416]
	cfldr64cs	mvdx9, [r0, #-1020]
	cfldr64ls	mvdx4, [r1, #-156]
	cfldr64le	mvdx7, [r9, #416]!
	cfldr64vs	mvdx11, [r0, #-1020]!
	cfldr64cc	mvdx12, [r1, #-156]!
	cfldr64	mvdx13, [r9, #416]!
	cfldr64cs	mvdx9, [r0, #-1020]!
	cfldr64ls	mvdx4, [r1], #-156
	cfldr64le	mvdx7, [r9], #416
	cfldr64vs	mvdx11, [r0], #-1020
	cfldr64cc	mvdx12, [r1], #-156
	cfldr64	mvdx13, [r9], #416
	cfstrscs	mvf9, [r0, #-1020]
	cfstrsls	mvf4, [r1, #-156]
	cfstrsle	mvf7, [r9, #416]
	cfstrsvs	mvf11, [r0, #-1020]
	cfstrscc	mvf12, [r1, #-156]
	cfstrs	mvf13, [r9, #416]!
	cfstrscs	mvf9, [r0, #-1020]!
	cfstrsls	mvf4, [r1, #-156]!
	cfstrsle	mvf7, [r9, #416]!
	cfstrsvs	mvf11, [r0, #-1020]!
	cfstrscc	mvf12, [r1], #-156
	cfstrs	mvf13, [r9], #416
	cfstrscs	mvf9, [r0], #-1020
	cfstrsls	mvf4, [r1], #-156
	cfstrsle	mvf7, [r9], #416
	cfstrdvs	mvd11, [r0, #-1020]
	cfstrdcc	mvd12, [r1, #-156]
	cfstrd	mvd13, [r9, #416]
	cfstrdcs	mvd9, [r0, #-1020]
	cfstrdls	mvd4, [r1, #-156]
	cfstrdle	mvd7, [r9, #416]!
	cfstrdvs	mvd11, [r0, #-1020]!
	cfstrdcc	mvd12, [r1, #-156]!
	cfstrd	mvd13, [r9, #416]!
	cfstrdcs	mvd9, [r0, #-1020]!
	cfstrdls	mvd4, [r1], #-156
	cfstrdle	mvd7, [r9], #416
	cfstrdvs	mvd11, [r0], #-1020
	cfstrdcc	mvd12, [r1], #-156
	cfstrd	mvd13, [r9], #416
	cfstr32cs	mvfx9, [r0, #-1020]
	cfstr32ls	mvfx4, [r1, #-156]
	cfstr32le	mvfx7, [r9, #416]
	cfstr32vs	mvfx11, [r0, #-1020]
	cfstr32cc	mvfx12, [r1, #-156]
	cfstr32	mvfx13, [r9, #416]!
	cfstr32cs	mvfx9, [r0, #-1020]!
	cfstr32ls	mvfx4, [r1, #-156]!
	cfstr32le	mvfx7, [r9, #416]!
	cfstr32vs	mvfx11, [r0, #-1020]!
	cfstr32cc	mvfx12, [r1], #-156
	cfstr32	mvfx13, [r9], #416
	cfstr32cs	mvfx9, [r0], #-1020
	cfstr32ls	mvfx4, [r1], #-156
	cfstr32le	mvfx7, [r9], #416
	cfstr64vs	mvdx11, [r0, #-1020]
	cfstr64cc	mvdx12, [r1, #-156]
	cfstr64	mvdx13, [r9, #416]
	cfstr64cs	mvdx9, [r0, #-1020]
	cfstr64ls	mvdx4, [r1, #-156]
	cfstr64le	mvdx7, [r9, #416]!
	cfstr64vs	mvdx11, [r0, #-1020]!
	cfstr64cc	mvdx12, [r1, #-156]!
	cfstr64	mvdx13, [r9, #416]!
	cfstr64cs	mvdx9, [r0, #-1020]!
	cfstr64ls	mvdx4, [r1], #-156
	cfstr64le	mvdx7, [r9], #416
	cfstr64vs	mvdx11, [r0], #-1020
	cfstr64cc	mvdx12, [r1], #-156
	cfstr64	mvdx13, [r9], #416
move:
	cfmvsrcs	mvf9, r0
	cfmvsrpl	mvf15, r7
	cfmvsrls	mvf4, r1
	cfmvsrcc	mvf8, r2
	cfmvsrvc	mvf2, r12
	cfmvrsgt	r9, mvf11
	cfmvrseq	sl, mvf5
	cfmvrsal	r4, mvf12
	cfmvrsge	fp, mvf8
	cfmvrs	r5, mvf6
	cfmvdlrlt	mvd4, r9
	cfmvdlrls	mvd0, r10
	cfmvdlr	mvd10, r4
	cfmvdlrmi	mvd14, r11
	cfmvdlrhi	mvd13, r5
	cfmvrdlcs	r12, mvd12
	cfmvrdlvs	r3, mvd0
	cfmvrdlvc	r13, mvd14
	cfmvrdlcc	r14, mvd10
	cfmvrdlne	r8, mvd15
	cfmvdhrle	mvd6, ip
	cfmvdhrmi	mvd2, r3
	cfmvdhreq	mvd5, sp
	cfmvdhrge	mvd9, lr
	cfmvdhral	mvd3, r8
	cfmvrdhle	r5, mvd2
	cfmvrdhne	r6, mvd6
	cfmvrdhlt	r0, mvd7
	cfmvrdhpl	r7, mvd3
	cfmvrdhgt	r1, mvd1
	cfmv64lrhi	mvdx15, r5
	cfmv64lrvs	mvdx11, r6
	cfmv64lrcs	mvdx9, r0
	cfmv64lrpl	mvdx15, r7
	cfmv64lrls	mvdx4, r1
	cfmvr64lcc	r8, mvdx13
	cfmvr64lvc	pc, mvdx1
	cfmvr64lgt	r9, mvdx11
	cfmvr64leq	sl, mvdx5
	cfmvr64lal	r4, mvdx12
	cfmv64hrge	mvdx1, r8
	cfmv64hr	mvdx13, r15
	cfmv64hrlt	mvdx4, r9
	cfmv64hrls	mvdx0, r10
	cfmv64hr	mvdx10, r4
	cfmvr64hmi	r1, mvdx3
	cfmvr64hhi	r2, mvdx7
	cfmvr64hcs	r12, mvdx12
	cfmvr64hvs	r3, mvdx0
	cfmvr64hvc	r13, mvdx14
	cfmval32cc	mvax0, mvfx10
	cfmval32ne	mvax1, mvfx15
	cfmval32le	mvax0, mvfx11
	cfmval32mi	mvax0, mvfx9
	cfmval32eq	mvax1, mvfx15
	cfmv32alge	mvfx9, mvax0
	cfmv32alal	mvfx3, mvax1
	cfmv32alle	mvfx7, mvax0
	cfmv32alne	mvfx12, mvax0
	cfmv32allt	mvfx0, mvax1
	cfmvam32pl	mvax2, mvfx3
	cfmvam32gt	mvax1, mvfx1
	cfmvam32hi	mvax3, mvfx13
	cfmvam32vs	mvax3, mvfx4
	cfmvam32cs	mvax1, mvfx0
	cfmv32ampl	mvfx15, mvax2
	cfmv32amls	mvfx4, mvax1
	cfmv32amcc	mvfx8, mvax3
	cfmv32amvc	mvfx2, mvax3
	cfmv32amgt	mvfx6, mvax1
	cfmvah32eq	mvax1, mvfx5
	cfmvah32al	mvax2, mvfx12
	cfmvah32ge	mvax3, mvfx8
	cfmvah32	mvax2, mvfx6
	cfmvah32lt	mvax2, mvfx2
	cfmv32ahls	mvfx0, mvax1
	cfmv32ah	mvfx10, mvax2
	cfmv32ahmi	mvfx14, mvax3
	cfmv32ahhi	mvfx13, mvax2
	cfmv32ahcs	mvfx1, mvax2
	cfmva32vs	mvax1, mvfx0
	cfmva32vc	mvax3, mvfx14
	cfmva32cc	mvax0, mvfx10
	cfmva32ne	mvax1, mvfx15
	cfmva32le	mvax0, mvfx11
	cfmv32ami	mvfx2, mvax1
	cfmv32aeq	mvfx5, mvax3
	cfmv32age	mvfx9, mvax0
	cfmv32aal	mvfx3, mvax1
	cfmv32ale	mvfx7, mvax0
	cfmva64ne	mvax2, mvdx6
	cfmva64lt	mvax0, mvdx7
	cfmva64pl	mvax2, mvdx3
	cfmva64gt	mvax1, mvdx1
	cfmva64hi	mvax3, mvdx13
	cfmv64avs	mvdx11, mvax2
	cfmv64acs	mvdx9, mvax0
	cfmv64apl	mvdx15, mvax2
	cfmv64als	mvdx4, mvax1
	cfmv64acc	mvdx8, mvax3
	cfmvsc32vc	dspsc, mvdx1
	cfmvsc32gt	dspsc, mvdx11
	cfmvsc32eq	dspsc, mvdx5
	cfmvsc32al	dspsc, mvdx12
	cfmvsc32ge	dspsc, mvdx8
	cfmv32sc	mvdx13, dspsc
	cfmv32sclt	mvdx4, dspsc
	cfmv32scls	mvdx0, dspsc
	cfmv32sc	mvdx10, dspsc
	cfmv32scmi	mvdx14, dspsc
	cfcpyshi	mvf13, mvf7
	cfcpyscs	mvf1, mvf12
	cfcpysvs	mvf11, mvf0
	cfcpysvc	mvf5, mvf14
	cfcpyscc	mvf12, mvf10
	cfcpydne	mvd8, mvd15
	cfcpydle	mvd6, mvd11
	cfcpydmi	mvd2, mvd9
	cfcpydeq	mvd5, mvd15
	cfcpydge	mvd9, mvd4
conv:
	cfcvtsdal	mvd3, mvf8
	cfcvtsdle	mvd7, mvf2
	cfcvtsdne	mvd12, mvf6
	cfcvtsdlt	mvd0, mvf7
	cfcvtsdpl	mvd14, mvf3
	cfcvtdsgt	mvf10, mvd1
	cfcvtdshi	mvf15, mvd13
	cfcvtdsvs	mvf11, mvd4
	cfcvtdscs	mvf9, mvd0
	cfcvtdspl	mvf15, mvd10
	cfcvt32sls	mvf4, mvfx14
	cfcvt32scc	mvf8, mvfx13
	cfcvt32svc	mvf2, mvfx1
	cfcvt32sgt	mvf6, mvfx11
	cfcvt32seq	mvf7, mvfx5
	cfcvt32dal	mvd3, mvfx12
	cfcvt32dge	mvd1, mvfx8
	cfcvt32d	mvd13, mvfx6
	cfcvt32dlt	mvd4, mvfx2
	cfcvt32dls	mvd0, mvfx5
	cfcvt64s	mvf10, mvdx9
	cfcvt64smi	mvf14, mvdx3
	cfcvt64shi	mvf13, mvdx7
	cfcvt64scs	mvf1, mvdx12
	cfcvt64svs	mvf11, mvdx0
	cfcvt64dvc	mvd5, mvdx14
	cfcvt64dcc	mvd12, mvdx10
	cfcvt64dne	mvd8, mvdx15
	cfcvt64dle	mvd6, mvdx11
	cfcvt64dmi	mvd2, mvdx9
	cfcvts32eq	mvfx5, mvf15
	cfcvts32ge	mvfx9, mvf4
	cfcvts32al	mvfx3, mvf8
	cfcvts32le	mvfx7, mvf2
	cfcvts32ne	mvfx12, mvf6
	cfcvtd32lt	mvfx0, mvd7
	cfcvtd32pl	mvfx14, mvd3
	cfcvtd32gt	mvfx10, mvd1
	cfcvtd32hi	mvfx15, mvd13
	cfcvtd32vs	mvfx11, mvd4
	cftruncs32cs	mvfx9, mvf0
	cftruncs32pl	mvfx15, mvf10
	cftruncs32ls	mvfx4, mvf14
	cftruncs32cc	mvfx8, mvf13
	cftruncs32vc	mvfx2, mvf1
	cftruncd32gt	mvfx6, mvd11
	cftruncd32eq	mvfx7, mvd5
	cftruncd32al	mvfx3, mvd12
	cftruncd32ge	mvfx1, mvd8
	cftruncd32	mvfx13, mvd6
shift:
	cfrshl32lt	mvfx4, mvfx2, r3
	cfrshl32pl	mvfx15, mvfx10, r4
	cfrshl32al	mvfx3, mvfx8, r2
	cfrshl32cs	mvfx1, mvfx12, r9
	cfrshl32eq	mvfx7, mvfx5, r7
	cfrshl64gt	mvdx10, mvdx1, r8
	cfrshl64le	mvdx6, mvdx11, r6
	cfrshl64ls	mvdx0, mvdx5, sp
	cfrshl64ls	mvdx4, mvdx14, r11
	cfrshl64le	mvdx7, mvdx2, r12
	cfsh32vs	mvfx11, mvfx0, #-1
	cfsh32al	mvfx3, mvfx12, #24
	cfsh32hi	mvfx15, mvfx13, #33
	cfsh32mi	mvfx2, mvfx9, #0
	cfsh32	mvfx10, mvfx9, #32
	cfsh64cc	mvdx8, mvdx13, #-31
	cfsh64ne	mvdx12, mvdx6, #1
	cfsh64vc	mvdx5, mvdx14, #-32
	cfsh64ge	mvdx1, mvdx8, #-27
	cfsh64vs	mvdx11, mvdx4, #-5
comp:
	cfcmpseq	r10, mvf15, mvf10
	cfcmpsmi	r1, mvf3, mvf8
	cfcmpsvc	pc, mvf1, mvf12
	cfcmpslt	r0, mvf7, mvf5
	cfcmpscc	r14, mvf10, mvf1
	cfcmpd	r5, mvd6, mvd11
	cfcmpdcs	r3, mvd0, mvd5
	cfcmpdge	r4, mvd4, mvd14
	cfcmpdhi	r2, mvd7, mvd2
	cfcmpdgt	r9, mvd11, mvd0
	cfcmp32pl	r7, mvfx3, mvfx12
	cfcmp32ne	r8, mvfx15, mvfx13
	cfcmp32lt	r6, mvfx2, mvfx9
	cfcmp32pl	sp, mvfx10, mvfx9
	cfcmp32al	r11, mvfx8, mvfx13
	cfcmp64cs	r12, mvdx12, mvdx6
	cfcmp64eq	sl, mvdx5, mvdx14
	cfcmp64gt	r1, mvdx1, mvdx8
	cfcmp64le	r15, mvdx11, mvdx4
	cfcmp64ls	r0, mvdx5, mvdx15
fp_arith:
	cfabssls	mvf4, mvf14
	cfabsscc	mvf8, mvf13
	cfabssvc	mvf2, mvf1
	cfabssgt	mvf6, mvf11
	cfabsseq	mvf7, mvf5
	cfabsdal	mvd3, mvd12
	cfabsdge	mvd1, mvd8
	cfabsd	mvd13, mvd6
	cfabsdlt	mvd4, mvd2
	cfabsdls	mvd0, mvd5
	cfnegs	mvf10, mvf9
	cfnegsmi	mvf14, mvf3
	cfnegshi	mvf13, mvf7
	cfnegscs	mvf1, mvf12
	cfnegsvs	mvf11, mvf0
	cfnegdvc	mvd5, mvd14
	cfnegdcc	mvd12, mvd10
	cfnegdne	mvd8, mvd15
	cfnegdle	mvd6, mvd11
	cfnegdmi	mvd2, mvd9
	cfaddseq	mvf5, mvf15, mvf10
	cfaddsmi	mvf14, mvf3, mvf8
	cfaddsvc	mvf2, mvf1, mvf12
	cfaddslt	mvf0, mvf7, mvf5
	cfaddscc	mvf12, mvf10, mvf1
	cfaddd	mvd13, mvd6, mvd11
	cfadddcs	mvd9, mvd0, mvd5
	cfadddge	mvd9, mvd4, mvd14
	cfadddhi	mvd13, mvd7, mvd2
	cfadddgt	mvd6, mvd11, mvd0
	cfsubspl	mvf14, mvf3, mvf12
	cfsubsne	mvf8, mvf15, mvf13
	cfsubslt	mvf4, mvf2, mvf9
	cfsubspl	mvf15, mvf10, mvf9
	cfsubsal	mvf3, mvf8, mvf13
	cfsubdcs	mvd1, mvd12, mvd6
	cfsubdeq	mvd7, mvd5, mvd14
	cfsubdgt	mvd10, mvd1, mvd8
	cfsubdle	mvd6, mvd11, mvd4
	cfsubdls	mvd0, mvd5, mvd15
	cfmulsls	mvf4, mvf14, mvf3
	cfmulsle	mvf7, mvf2, mvf1
	cfmulsvs	mvf11, mvf0, mvf7
	cfmulsal	mvf3, mvf12, mvf10
	cfmulshi	mvf15, mvf13, mvf6
	cfmuldmi	mvd2, mvd9, mvd0
	cfmuld	mvd10, mvd9, mvd4
	cfmuldcc	mvd8, mvd13, mvd7
	cfmuldne	mvd12, mvd6, mvd11
	cfmuldvc	mvd5, mvd14, mvd3
int_arith:
	cfabs32ge	mvfx1, mvfx8
	cfabs32	mvfx13, mvfx6
	cfabs32lt	mvfx4, mvfx2
	cfabs32ls	mvfx0, mvfx5
	cfabs32	mvfx10, mvfx9
	cfabs64mi	mvdx14, mvdx3
	cfabs64hi	mvdx13, mvdx7
	cfabs64cs	mvdx1, mvdx12
	cfabs64vs	mvdx11, mvdx0
	cfabs64vc	mvdx5, mvdx14
	cfneg32cc	mvfx12, mvfx10
	cfneg32ne	mvfx8, mvfx15
	cfneg32le	mvfx6, mvfx11
	cfneg32mi	mvfx2, mvfx9
	cfneg32eq	mvfx5, mvfx15
	cfneg64ge	mvdx9, mvdx4
	cfneg64al	mvdx3, mvdx8
	cfneg64le	mvdx7, mvdx2
	cfneg64ne	mvdx12, mvdx6
	cfneg64lt	mvdx0, mvdx7
	cfadd32pl	mvfx14, mvfx3, mvfx12
	cfadd32ne	mvfx8, mvfx15, mvfx13
	cfadd32lt	mvfx4, mvfx2, mvfx9
	cfadd32pl	mvfx15, mvfx10, mvfx9
	cfadd32al	mvfx3, mvfx8, mvfx13
	cfadd64cs	mvdx1, mvdx12, mvdx6
	cfadd64eq	mvdx7, mvdx5, mvdx14
	cfadd64gt	mvdx10, mvdx1, mvdx8
	cfadd64le	mvdx6, mvdx11, mvdx4
	cfadd64ls	mvdx0, mvdx5, mvdx15
	cfsub32ls	mvfx4, mvfx14, mvfx3
	cfsub32le	mvfx7, mvfx2, mvfx1
	cfsub32vs	mvfx11, mvfx0, mvfx7
	cfsub32al	mvfx3, mvfx12, mvfx10
	cfsub32hi	mvfx15, mvfx13, mvfx6
	cfsub64mi	mvdx2, mvdx9, mvdx0
	cfsub64	mvdx10, mvdx9, mvdx4
	cfsub64cc	mvdx8, mvdx13, mvdx7
	cfsub64ne	mvdx12, mvdx6, mvdx11
	cfsub64vc	mvdx5, mvdx14, mvdx3
	cfmul32ge	mvfx1, mvfx8, mvfx15
	cfmul32vs	mvfx11, mvfx4, mvfx2
	cfmul32eq	mvfx5, mvfx15, mvfx10
	cfmul32mi	mvfx14, mvfx3, mvfx8
	cfmul32vc	mvfx2, mvfx1, mvfx12
	cfmul64lt	mvdx0, mvdx7, mvdx5
	cfmul64cc	mvdx12, mvdx10, mvdx1
	cfmul64	mvdx13, mvdx6, mvdx11
	cfmul64cs	mvdx9, mvdx0, mvdx5
	cfmul64ge	mvdx9, mvdx4, mvdx14
	cfmac32hi	mvfx13, mvfx7, mvfx2
	cfmac32gt	mvfx6, mvfx11, mvfx0
	cfmac32pl	mvfx14, mvfx3, mvfx12
	cfmac32ne	mvfx8, mvfx15, mvfx13
	cfmac32lt	mvfx4, mvfx2, mvfx9
	cfmsc32pl	mvfx15, mvfx10, mvfx9
	cfmsc32al	mvfx3, mvfx8, mvfx13
	cfmsc32cs	mvfx1, mvfx12, mvfx6
	cfmsc32eq	mvfx7, mvfx5, mvfx14
	cfmsc32gt	mvfx10, mvfx1, mvfx8
acc_arith:
	cfmadd32le	mvax0, mvfx11, mvfx4, mvfx2
	cfmadd32ls	mvax0, mvfx5, mvfx15, mvfx10
	cfmadd32ls	mvax0, mvfx14, mvfx3, mvfx8
	cfmadd32le	mvax2, mvfx2, mvfx1, mvfx12
	cfmadd32vs	mvax1, mvfx0, mvfx7, mvfx5
	cfmsub32al	mvax2, mvfx12, mvfx10, mvfx1
	cfmsub32hi	mvax3, mvfx13, mvfx6, mvfx11
	cfmsub32mi	mvax0, mvfx9, mvfx0, mvfx5
	cfmsub32	mvax2, mvfx9, mvfx4, mvfx14
	cfmsub32cc	mvax1, mvfx13, mvfx7, mvfx2
	cfmadda32ne	mvax2, mvax0, mvfx11, mvfx0
	cfmadda32vc	mvax3, mvax2, mvfx3, mvfx12
	cfmadda32ge	mvax3, mvax1, mvfx15, mvfx13
	cfmadda32vs	mvax3, mvax2, mvfx2, mvfx9
	cfmadda32eq	mvax1, mvax3, mvfx10, mvfx9
	cfmsuba32mi	mvax1, mvax3, mvfx8, mvfx13
	cfmsuba32vc	mvax0, mvax3, mvfx12, mvfx6
	cfmsuba32lt	mvax0, mvax1, mvfx5, mvfx14
	cfmsuba32cc	mvax0, mvax1, mvfx1, mvfx8
	cfmsuba32	mvax2, mvax0, mvfx11, mvfx4
