#objdump: -dr --prefix-addresses --show-raw-insn -M reg-names=numeric
#name: MIPS mips5 instructions
#stderr: mips5.l

# Check MIPS V instruction assembly

.*: +file format .*mips.*

Disassembly of section \.text:
0+0000 <[^>]*> 46c01005 	abs\.ps	\$f0,\$f2
0+0004 <[^>]*> 46c62080 	add\.ps	\$f2,\$f4,\$f6
0+0008 <[^>]*> 4c6a419e 	alnv\.ps	\$f6,\$f8,\$f10,\$3
0+000c <[^>]*> 46ca4032 	c\.eq\.ps	\$f8,\$f10
0+0010 <[^>]*> 46cc5232 	c\.eq\.ps	\$fcc2,\$f10,\$f12
0+0014 <[^>]*> 46ca4030 	c\.f\.ps	\$f8,\$f10
0+0018 <[^>]*> 46cc5230 	c\.f\.ps	\$fcc2,\$f10,\$f12
0+001c <[^>]*> 46ca403e 	c\.le\.ps	\$f8,\$f10
0+0020 <[^>]*> 46cc523e 	c\.le\.ps	\$fcc2,\$f10,\$f12
0+0024 <[^>]*> 46ca403c 	c\.lt\.ps	\$f8,\$f10
0+0028 <[^>]*> 46cc523c 	c\.lt\.ps	\$fcc2,\$f10,\$f12
0+002c <[^>]*> 46ca403d 	c\.nge\.ps	\$f8,\$f10
0+0030 <[^>]*> 46cc523d 	c\.nge\.ps	\$fcc2,\$f10,\$f12
0+0034 <[^>]*> 46ca403b 	c\.ngl\.ps	\$f8,\$f10
0+0038 <[^>]*> 46cc523b 	c\.ngl\.ps	\$fcc2,\$f10,\$f12
0+003c <[^>]*> 46ca4039 	c\.ngle\.ps	\$f8,\$f10
0+0040 <[^>]*> 46cc5239 	c\.ngle\.ps	\$fcc2,\$f10,\$f12
0+0044 <[^>]*> 46ca403f 	c\.ngt\.ps	\$f8,\$f10
0+0048 <[^>]*> 46cc523f 	c\.ngt\.ps	\$fcc2,\$f10,\$f12
0+004c <[^>]*> 46ca4036 	c\.ole\.ps	\$f8,\$f10
0+0050 <[^>]*> 46cc5236 	c\.ole\.ps	\$fcc2,\$f10,\$f12
0+0054 <[^>]*> 46ca4034 	c\.olt\.ps	\$f8,\$f10
0+0058 <[^>]*> 46cc5234 	c\.olt\.ps	\$fcc2,\$f10,\$f12
0+005c <[^>]*> 46ca403a 	c\.seq\.ps	\$f8,\$f10
0+0060 <[^>]*> 46cc523a 	c\.seq\.ps	\$fcc2,\$f10,\$f12
0+0064 <[^>]*> 46ca4038 	c\.sf\.ps	\$f8,\$f10
0+0068 <[^>]*> 46cc5238 	c\.sf\.ps	\$fcc2,\$f10,\$f12
0+006c <[^>]*> 46ca4033 	c\.ueq\.ps	\$f8,\$f10
0+0070 <[^>]*> 46cc5233 	c\.ueq\.ps	\$fcc2,\$f10,\$f12
0+0074 <[^>]*> 46ca4037 	c\.ule\.ps	\$f8,\$f10
0+0078 <[^>]*> 46cc5237 	c\.ule\.ps	\$fcc2,\$f10,\$f12
0+007c <[^>]*> 46ca4035 	c\.ult\.ps	\$f8,\$f10
0+0080 <[^>]*> 46cc5235 	c\.ult\.ps	\$fcc2,\$f10,\$f12
0+0084 <[^>]*> 46ca4031 	c\.un\.ps	\$f8,\$f10
0+0088 <[^>]*> 46cc5231 	c\.un\.ps	\$fcc2,\$f10,\$f12
0+008c <[^>]*> 46107326 	cvt\.ps\.s	\$f12,\$f14,\$f16
0+0090 <[^>]*> 46c09428 	cvt\.s\.pl	\$f16,\$f18
0+0094 <[^>]*> 46c0a4a0 	cvt\.s\.pu	\$f18,\$f20
0+0098 <[^>]*> 4ca40505 	luxc1	\$f20,\$4\(\$5\)
0+009c <[^>]*> 4edac526 	madd\.ps	\$f20,\$f22,\$f24,\$f26
0+00a0 <[^>]*> 46c0d606 	mov\.ps	\$f24,\$f26
0+00a4 <[^>]*> 46c8e691 	movf\.ps	\$f26,\$f28,\$fcc2
0+00a8 <[^>]*> 46c3e693 	movn\.ps	\$f26,\$f28,\$3
0+00ac <[^>]*> 46d1f711 	movt\.ps	\$f28,\$f30,\$fcc4
0+00b0 <[^>]*> 46c5f712 	movz\.ps	\$f28,\$f30,\$5
0+00b4 <[^>]*> 4c0417ae 	msub\.ps	\$f30,\$f0,\$f2,\$f4
0+00b8 <[^>]*> 46c62082 	mul\.ps	\$f2,\$f4,\$f6
0+00bc <[^>]*> 46c04187 	neg\.ps	\$f6,\$f8
0+00c0 <[^>]*> 4d0c51b6 	nmadd\.ps	\$f6,\$f8,\$f10,\$f12
0+00c4 <[^>]*> 4d0c51be 	nmsub\.ps	\$f6,\$f8,\$f10,\$f12
0+00c8 <[^>]*> 46ce62ac 	pll\.ps	\$f10,\$f12,\$f14
0+00cc <[^>]*> 46d283ad 	plu\.ps	\$f14,\$f16,\$f18
0+00d0 <[^>]*> 46d4942e 	pul\.ps	\$f16,\$f18,\$f20
0+00d4 <[^>]*> 46d8b52f 	puu\.ps	\$f20,\$f22,\$f24
0+00d8 <[^>]*> 46dac581 	sub\.ps	\$f22,\$f24,\$f26
0+00dc <[^>]*> 4ce6d00d 	suxc1	\$f26,\$6\(\$7\)
0+00e0 <[^>]*> 46cc5332 	c\.eq\.ps	\$fcc3,\$f10,\$f12
0+00e4 <[^>]*> 46cce691 	movf\.ps	\$f26,\$f28,\$fcc3
	\.\.\.
