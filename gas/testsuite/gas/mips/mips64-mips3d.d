#objdump: -dr --prefix-addresses --show-raw-insn -mmips:isa64
#name: MIPS MIPS64 MIPS-3D ASE instructions (-mips3d flag)
#as: -mips64 -mips3d
#stderr: mips64-mips3d.l

# Check MIPS64 MIPS-3D ASE instruction assembly and disassembly

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> 46d34118 	addr\.ps	\$f4,\$f8,\$f19
0+0004 <[^>]*> 4520fffe 	bc1any2f	\$fcc0,0+0000 <text_label>
0+0008 <[^>]*> 00000000 	nop
0+000c <[^>]*> 4528fffc 	bc1any2f	\$fcc2,0+0000 <text_label>
0+0010 <[^>]*> 00000000 	nop
0+0014 <[^>]*> 4521fffa 	bc1any2t	\$fcc0,0+0000 <text_label>
0+0018 <[^>]*> 00000000 	nop
0+001c <[^>]*> 4531fff8 	bc1any2t	\$fcc4,0+0000 <text_label>
0+0020 <[^>]*> 00000000 	nop
0+0024 <[^>]*> 4540fff6 	bc1any4f	\$fcc0,0+0000 <text_label>
0+0028 <[^>]*> 00000000 	nop
0+002c <[^>]*> 4550fff4 	bc1any4f	\$fcc4,0+0000 <text_label>
0+0030 <[^>]*> 00000000 	nop
0+0034 <[^>]*> 4541fff2 	bc1any4t	\$fcc0,0+0000 <text_label>
0+0038 <[^>]*> 00000000 	nop
0+003c <[^>]*> 4551fff0 	bc1any4t	\$fcc4,0+0000 <text_label>
0+0040 <[^>]*> 00000000 	nop
0+0044 <[^>]*> 46334070 	cabs\.f\.d	\$fcc0,\$f8,\$f19
0+0048 <[^>]*> 46334270 	cabs\.f\.d	\$fcc2,\$f8,\$f19
0+004c <[^>]*> 46134070 	cabs\.f\.s	\$fcc0,\$f8,\$f19
0+0050 <[^>]*> 46134270 	cabs\.f\.s	\$fcc2,\$f8,\$f19
0+0054 <[^>]*> 46d34070 	cabs\.f\.ps	\$fcc0,\$f8,\$f19
0+0058 <[^>]*> 46d34270 	cabs\.f\.ps	\$fcc2,\$f8,\$f19
0+005c <[^>]*> 46334071 	cabs\.un\.d	\$fcc0,\$f8,\$f19
0+0060 <[^>]*> 46334271 	cabs\.un\.d	\$fcc2,\$f8,\$f19
0+0064 <[^>]*> 46134071 	cabs\.un\.s	\$fcc0,\$f8,\$f19
0+0068 <[^>]*> 46134271 	cabs\.un\.s	\$fcc2,\$f8,\$f19
0+006c <[^>]*> 46d34071 	cabs\.un\.ps	\$fcc0,\$f8,\$f19
0+0070 <[^>]*> 46d34271 	cabs\.un\.ps	\$fcc2,\$f8,\$f19
0+0074 <[^>]*> 46334072 	cabs\.eq\.d	\$fcc0,\$f8,\$f19
0+0078 <[^>]*> 46334272 	cabs\.eq\.d	\$fcc2,\$f8,\$f19
0+007c <[^>]*> 46134072 	cabs\.eq\.s	\$fcc0,\$f8,\$f19
0+0080 <[^>]*> 46134272 	cabs\.eq\.s	\$fcc2,\$f8,\$f19
0+0084 <[^>]*> 46d34072 	cabs\.eq\.ps	\$fcc0,\$f8,\$f19
0+0088 <[^>]*> 46d34272 	cabs\.eq\.ps	\$fcc2,\$f8,\$f19
0+008c <[^>]*> 46334073 	cabs\.ueq\.d	\$fcc0,\$f8,\$f19
0+0090 <[^>]*> 46334273 	cabs\.ueq\.d	\$fcc2,\$f8,\$f19
0+0094 <[^>]*> 46134073 	cabs\.ueq\.s	\$fcc0,\$f8,\$f19
0+0098 <[^>]*> 46134273 	cabs\.ueq\.s	\$fcc2,\$f8,\$f19
0+009c <[^>]*> 46d34073 	cabs\.ueq\.ps	\$fcc0,\$f8,\$f19
0+00a0 <[^>]*> 46d34273 	cabs\.ueq\.ps	\$fcc2,\$f8,\$f19
0+00a4 <[^>]*> 46334074 	cabs\.olt\.d	\$fcc0,\$f8,\$f19
0+00a8 <[^>]*> 46334274 	cabs\.olt\.d	\$fcc2,\$f8,\$f19
0+00ac <[^>]*> 46134074 	cabs\.olt\.s	\$fcc0,\$f8,\$f19
0+00b0 <[^>]*> 46134274 	cabs\.olt\.s	\$fcc2,\$f8,\$f19
0+00b4 <[^>]*> 46d34074 	cabs\.olt\.ps	\$fcc0,\$f8,\$f19
0+00b8 <[^>]*> 46d34274 	cabs\.olt\.ps	\$fcc2,\$f8,\$f19
0+00bc <[^>]*> 46334075 	cabs\.ult\.d	\$fcc0,\$f8,\$f19
0+00c0 <[^>]*> 46334275 	cabs\.ult\.d	\$fcc2,\$f8,\$f19
0+00c4 <[^>]*> 46134075 	cabs\.ult\.s	\$fcc0,\$f8,\$f19
0+00c8 <[^>]*> 46134275 	cabs\.ult\.s	\$fcc2,\$f8,\$f19
0+00cc <[^>]*> 46d34075 	cabs\.ult\.ps	\$fcc0,\$f8,\$f19
0+00d0 <[^>]*> 46d34275 	cabs\.ult\.ps	\$fcc2,\$f8,\$f19
0+00d4 <[^>]*> 46334076 	cabs\.ole\.d	\$fcc0,\$f8,\$f19
0+00d8 <[^>]*> 46334276 	cabs\.ole\.d	\$fcc2,\$f8,\$f19
0+00dc <[^>]*> 46134076 	cabs\.ole\.s	\$fcc0,\$f8,\$f19
0+00e0 <[^>]*> 46134276 	cabs\.ole\.s	\$fcc2,\$f8,\$f19
0+00e4 <[^>]*> 46d34076 	cabs\.ole\.ps	\$fcc0,\$f8,\$f19
0+00e8 <[^>]*> 46d34276 	cabs\.ole\.ps	\$fcc2,\$f8,\$f19
0+00ec <[^>]*> 46334077 	cabs\.ule\.d	\$fcc0,\$f8,\$f19
0+00f0 <[^>]*> 46334277 	cabs\.ule\.d	\$fcc2,\$f8,\$f19
0+00f4 <[^>]*> 46134077 	cabs\.ule\.s	\$fcc0,\$f8,\$f19
0+00f8 <[^>]*> 46134277 	cabs\.ule\.s	\$fcc2,\$f8,\$f19
0+00fc <[^>]*> 46d34077 	cabs\.ule\.ps	\$fcc0,\$f8,\$f19
0+0100 <[^>]*> 46d34277 	cabs\.ule\.ps	\$fcc2,\$f8,\$f19
0+0104 <[^>]*> 46334078 	cabs\.sf\.d	\$fcc0,\$f8,\$f19
0+0108 <[^>]*> 46334278 	cabs\.sf\.d	\$fcc2,\$f8,\$f19
0+010c <[^>]*> 46134078 	cabs\.sf\.s	\$fcc0,\$f8,\$f19
0+0110 <[^>]*> 46134278 	cabs\.sf\.s	\$fcc2,\$f8,\$f19
0+0114 <[^>]*> 46d34078 	cabs\.sf\.ps	\$fcc0,\$f8,\$f19
0+0118 <[^>]*> 46d34278 	cabs\.sf\.ps	\$fcc2,\$f8,\$f19
0+011c <[^>]*> 46334079 	cabs\.ngle\.d	\$fcc0,\$f8,\$f19
0+0120 <[^>]*> 46334279 	cabs\.ngle\.d	\$fcc2,\$f8,\$f19
0+0124 <[^>]*> 46134079 	cabs\.ngle\.s	\$fcc0,\$f8,\$f19
0+0128 <[^>]*> 46134279 	cabs\.ngle\.s	\$fcc2,\$f8,\$f19
0+012c <[^>]*> 46d34079 	cabs\.ngle\.ps	\$fcc0,\$f8,\$f19
0+0130 <[^>]*> 46d34279 	cabs\.ngle\.ps	\$fcc2,\$f8,\$f19
0+0134 <[^>]*> 4633407a 	cabs\.seq\.d	\$fcc0,\$f8,\$f19
0+0138 <[^>]*> 4633427a 	cabs\.seq\.d	\$fcc2,\$f8,\$f19
0+013c <[^>]*> 4613407a 	cabs\.seq\.s	\$fcc0,\$f8,\$f19
0+0140 <[^>]*> 4613427a 	cabs\.seq\.s	\$fcc2,\$f8,\$f19
0+0144 <[^>]*> 46d3407a 	cabs\.seq\.ps	\$fcc0,\$f8,\$f19
0+0148 <[^>]*> 46d3427a 	cabs\.seq\.ps	\$fcc2,\$f8,\$f19
0+014c <[^>]*> 4633407b 	cabs\.ngl\.d	\$fcc0,\$f8,\$f19
0+0150 <[^>]*> 4633427b 	cabs\.ngl\.d	\$fcc2,\$f8,\$f19
0+0154 <[^>]*> 4613407b 	cabs\.ngl\.s	\$fcc0,\$f8,\$f19
0+0158 <[^>]*> 4613427b 	cabs\.ngl\.s	\$fcc2,\$f8,\$f19
0+015c <[^>]*> 46d3407b 	cabs\.ngl\.ps	\$fcc0,\$f8,\$f19
0+0160 <[^>]*> 46d3427b 	cabs\.ngl\.ps	\$fcc2,\$f8,\$f19
0+0164 <[^>]*> 4633407c 	cabs\.lt\.d	\$fcc0,\$f8,\$f19
0+0168 <[^>]*> 4633427c 	cabs\.lt\.d	\$fcc2,\$f8,\$f19
0+016c <[^>]*> 4613407c 	cabs\.lt\.s	\$fcc0,\$f8,\$f19
0+0170 <[^>]*> 4613427c 	cabs\.lt\.s	\$fcc2,\$f8,\$f19
0+0174 <[^>]*> 46d3407c 	cabs\.lt\.ps	\$fcc0,\$f8,\$f19
0+0178 <[^>]*> 46d3427c 	cabs\.lt\.ps	\$fcc2,\$f8,\$f19
0+017c <[^>]*> 4633407d 	cabs\.nge\.d	\$fcc0,\$f8,\$f19
0+0180 <[^>]*> 4633427d 	cabs\.nge\.d	\$fcc2,\$f8,\$f19
0+0184 <[^>]*> 4613407d 	cabs\.nge\.s	\$fcc0,\$f8,\$f19
0+0188 <[^>]*> 4613427d 	cabs\.nge\.s	\$fcc2,\$f8,\$f19
0+018c <[^>]*> 46d3407d 	cabs\.nge\.ps	\$fcc0,\$f8,\$f19
0+0190 <[^>]*> 46d3427d 	cabs\.nge\.ps	\$fcc2,\$f8,\$f19
0+0194 <[^>]*> 4633407e 	cabs\.le\.d	\$fcc0,\$f8,\$f19
0+0198 <[^>]*> 4633427e 	cabs\.le\.d	\$fcc2,\$f8,\$f19
0+019c <[^>]*> 4613407e 	cabs\.le\.s	\$fcc0,\$f8,\$f19
0+01a0 <[^>]*> 4613427e 	cabs\.le\.s	\$fcc2,\$f8,\$f19
0+01a4 <[^>]*> 46d3407e 	cabs\.le\.ps	\$fcc0,\$f8,\$f19
0+01a8 <[^>]*> 46d3427e 	cabs\.le\.ps	\$fcc2,\$f8,\$f19
0+01ac <[^>]*> 4633407f 	cabs\.ngt\.d	\$fcc0,\$f8,\$f19
0+01b0 <[^>]*> 4633427f 	cabs\.ngt\.d	\$fcc2,\$f8,\$f19
0+01b4 <[^>]*> 4613407f 	cabs\.ngt\.s	\$fcc0,\$f8,\$f19
0+01b8 <[^>]*> 4613427f 	cabs\.ngt\.s	\$fcc2,\$f8,\$f19
0+01bc <[^>]*> 46d3407f 	cabs\.ngt\.ps	\$fcc0,\$f8,\$f19
0+01c0 <[^>]*> 46d3427f 	cabs\.ngt\.ps	\$fcc2,\$f8,\$f19
0+01c4 <[^>]*> 46c09924 	cvt\.pw\.ps	\$f4,\$f19
0+01c8 <[^>]*> 46809926 	cvt\.ps\.pw	\$f4,\$f19
0+01cc <[^>]*> 46d3411a 	mulr\.ps	\$f4,\$f8,\$f19
0+01d0 <[^>]*> 46209a1d 	recip1\.d	\$f8,\$f19
0+01d4 <[^>]*> 46009a1d 	recip1\.s	\$f8,\$f19
0+01d8 <[^>]*> 46c09a1d 	recip1\.ps	\$f8,\$f19
0+01dc <[^>]*> 4633411c 	recip2\.d	\$f4,\$f8,\$f19
0+01e0 <[^>]*> 4613411c 	recip2\.s	\$f4,\$f8,\$f19
0+01e4 <[^>]*> 46d3411c 	recip2\.ps	\$f4,\$f8,\$f19
0+01e8 <[^>]*> 46209a1e 	rsqrt1\.d	\$f8,\$f19
0+01ec <[^>]*> 46009a1e 	rsqrt1\.s	\$f8,\$f19
0+01f0 <[^>]*> 46c09a1e 	rsqrt1\.ps	\$f8,\$f19
0+01f4 <[^>]*> 4633411f 	rsqrt2\.d	\$f4,\$f8,\$f19
0+01f8 <[^>]*> 4613411f 	rsqrt2\.s	\$f4,\$f8,\$f19
0+01fc <[^>]*> 46d3411f 	rsqrt2\.ps	\$f4,\$f8,\$f19
0+0200 <[^>]*> 4524ff7f 	bc1any2f	\$fcc1,0+0000 <text_label>
0+0204 <[^>]*> 00000000 	nop
0+0208 <[^>]*> 452dff7d 	bc1any2t	\$fcc3,0+0000 <text_label>
0+020c <[^>]*> 00000000 	nop
0+0210 <[^>]*> 4544ff7b 	bc1any4f	\$fcc1,0+0000 <text_label>
0+0214 <[^>]*> 00000000 	nop
0+0218 <[^>]*> 4549ff79 	bc1any4t	\$fcc2,0+0000 <text_label>
0+021c <[^>]*> 00000000 	nop
	\.\.\.
