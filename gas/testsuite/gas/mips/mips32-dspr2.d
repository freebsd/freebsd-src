#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS DSP ASE Rev2 for MIPS32
#as: -mdspr2 -32

# Check MIPS DSP ASE Rev2 for MIPS32 Instruction Assembly

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> 7c010052 	absq_s\.qb	zero,at
0+0004 <[^>]*> 7c430a10 	addu\.ph	at,v0,v1
0+0008 <[^>]*> 7c641310 	addu_s\.ph	v0,v1,a0
0+000c <[^>]*> 7c851818 	adduh\.qb	v1,a0,a1
0+0010 <[^>]*> 7ca62098 	adduh_r\.qb	a0,a1,a2
0+0014 <[^>]*> 7cc50031 	append	a1,a2,0x0
0+0018 <[^>]*> 7cc5f831 	append	a1,a2,0x1f
0+001c <[^>]*> 00000000 	nop
0+0020 <[^>]*> 7ce60c31 	balign	a2,a3,0x1
0+0024 <[^>]*> 7cc73391 	packrl.ph	a2,a2,a3
0+0028 <[^>]*> 7ce61c31 	balign	a2,a3,0x3
0+002c <[^>]*> 7ce83611 	cmpgdu\.eq\.qb	a2,a3,t0
0+0030 <[^>]*> 7d093e51 	cmpgdu\.lt\.qb	a3,t0,t1
0+0034 <[^>]*> 7d2a4691 	cmpgdu\.le\.qb	t0,t1,t2
0+0038 <[^>]*> 7d2a0030 	dpa\.w\.ph	\$ac0,t1,t2
0+003c <[^>]*> 7d4b0870 	dps\.w\.ph	\$ac1,t2,t3
0+0040 <[^>]*> 716c1000 	madd	\$ac2,t3,t4
0+0044 <[^>]*> 718d1801 	maddu	\$ac3,t4,t5
0+0048 <[^>]*> 71ae0004 	msub	t5,t6
0+004c <[^>]*> 71cf0805 	msubu	\$ac1,t6,t7
0+0050 <[^>]*> 7e117b18 	mul\.ph	t7,s0,s1
0+0054 <[^>]*> 7e328398 	mul_s\.ph	s0,s1,s2
0+0058 <[^>]*> 7e538dd8 	mulq_rs\.w	s1,s2,s3
0+005c <[^>]*> 7e749790 	mulq_s\.ph	s2,s3,s4
0+0060 <[^>]*> 7e959d98 	mulq_s\.w	s3,s4,s5
0+0064 <[^>]*> 7e9510b0 	mulsa\.w\.ph	\$ac2,s4,s5
0+0068 <[^>]*> 02b61818 	mult	\$ac3,s5,s6
0+006c <[^>]*> 02d70019 	multu	s6,s7
0+0070 <[^>]*> 7f19bb51 	precr\.qb\.ph	s7,t8,t9
0+0074 <[^>]*> 7f380791 	precr_sra\.ph\.w	t8,t9,0x0
0+0078 <[^>]*> 7f38ff91 	precr_sra\.ph\.w	t8,t9,0x1f
0+007c <[^>]*> 7f5907d1 	precr_sra_r\.ph\.w	t9,k0,0x0
0+0080 <[^>]*> 7f59ffd1 	precr_sra_r\.ph\.w	t9,k0,0x1f
0+0084 <[^>]*> 7f7a0071 	prepend	k0,k1,0x0
0+0088 <[^>]*> 7f7af871 	prepend	k0,k1,0x1f
0+008c <[^>]*> 7c1cd913 	shra\.qb	k1,gp,0x0
0+0090 <[^>]*> 7cfcd913 	shra\.qb	k1,gp,0x7
0+0094 <[^>]*> 7c1de153 	shra_r\.qb	gp,sp,0x0
0+0098 <[^>]*> 7cfde153 	shra_r\.qb	gp,sp,0x7
0+009c <[^>]*> 7ffee993 	shrav\.qb	sp,s8,ra
0+00a0 <[^>]*> 7c1ff1d3 	shrav_r\.qb	s8,ra,zero
0+00a4 <[^>]*> 7c00fe53 	shrl\.ph	ra,zero,0x0
0+00a8 <[^>]*> 7de0fe53 	shrl\.ph	ra,zero,0xf
0+00ac <[^>]*> 7c4106d3 	shrlv\.ph	zero,at,v0
0+00b0 <[^>]*> 7c430a50 	subu\.ph	at,v0,v1
0+00b4 <[^>]*> 7c641350 	subu_s\.ph	v0,v1,a0
0+00b8 <[^>]*> 7c851858 	subuh\.qb	v1,a0,a1
0+00bc <[^>]*> 7ca620d8 	subuh_r\.qb	a0,a1,a2
0+00c0 <[^>]*> 7cc72a18 	addqh\.ph	a1,a2,a3
0+00c4 <[^>]*> 7ce83298 	addqh_r\.ph	a2,a3,t0
0+00c8 <[^>]*> 7d093c18 	addqh\.w	a3,t0,t1
0+00cc <[^>]*> 7d2a4498 	addqh_r\.w	t0,t1,t2
0+00d0 <[^>]*> 7d4b4a58 	subqh\.ph	t1,t2,t3
0+00d4 <[^>]*> 7d6c52d8 	subqh_r\.ph	t2,t3,t4
0+00d8 <[^>]*> 7d8d5c58 	subqh\.w	t3,t4,t5
0+00dc <[^>]*> 7dae64d8 	subqh_r\.w	t4,t5,t6
0+00e0 <[^>]*> 7dae0a30 	dpax\.w\.ph	\$ac1,t5,t6
0+00e4 <[^>]*> 7dcf1270 	dpsx\.w\.ph	\$ac2,t6,t7
0+00e8 <[^>]*> 7df01e30 	dpaqx_s\.w\.ph	\$ac3,t7,s0
0+00ec <[^>]*> 7e1106b0 	dpaqx_sa\.w\.ph	\$ac0,s0,s1
0+00f0 <[^>]*> 7e320e70 	dpsqx_s\.w\.ph	\$ac1,s1,s2
0+00f4 <[^>]*> 7e5316f0 	dpsqx_sa\.w\.ph	\$ac2,s2,s3
	\.\.\.
