#objdump: -dr --prefix-addresses --show-raw-insn
#name: VFP Double-precision instructions
#as: -mfpu=vfp

# Test the ARM VFP Double Precision instructions

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]*> eeb40bc0 	fcmped	d0, d0
0+004 <[^>]*> eeb50bc0 	fcmpezd	d0
0+008 <[^>]*> eeb40b40 	fcmpd	d0, d0
0+00c <[^>]*> eeb50b40 	fcmpzd	d0
0+010 <[^>]*> eeb00bc0 	fabsd	d0, d0
0+014 <[^>]*> eeb00b40 	fcpyd	d0, d0
0+018 <[^>]*> eeb10b40 	fnegd	d0, d0
0+01c <[^>]*> eeb10bc0 	fsqrtd	d0, d0
0+020 <[^>]*> ee300b00 	faddd	d0, d0, d0
0+024 <[^>]*> ee800b00 	fdivd	d0, d0, d0
0+028 <[^>]*> ee000b00 	fmacd	d0, d0, d0
0+02c <[^>]*> ee100b00 	fmscd	d0, d0, d0
0+030 <[^>]*> ee200b00 	fmuld	d0, d0, d0
0+034 <[^>]*> ee000b40 	fnmacd	d0, d0, d0
0+038 <[^>]*> ee100b40 	fnmscd	d0, d0, d0
0+03c <[^>]*> ee200b40 	fnmuld	d0, d0, d0
0+040 <[^>]*> ee300b40 	fsubd	d0, d0, d0
0+044 <[^>]*> ed900b00 	vldr	d0, \[r0\]
0+048 <[^>]*> ed800b00 	vstr	d0, \[r0\]
0+04c <[^>]*> ec900b02 	vldmia	r0, {d0}
0+050 <[^>]*> ec900b02 	vldmia	r0, {d0}
0+054 <[^>]*> ecb00b02 	vldmia	r0!, {d0}
0+058 <[^>]*> ecb00b02 	vldmia	r0!, {d0}
0+05c <[^>]*> ed300b02 	vldmdb	r0!, {d0}
0+060 <[^>]*> ed300b02 	vldmdb	r0!, {d0}
0+064 <[^>]*> ec800b02 	vstmia	r0, {d0}
0+068 <[^>]*> ec800b02 	vstmia	r0, {d0}
0+06c <[^>]*> eca00b02 	vstmia	r0!, {d0}
0+070 <[^>]*> eca00b02 	vstmia	r0!, {d0}
0+074 <[^>]*> ed200b02 	vstmdb	r0!, {d0}
0+078 <[^>]*> ed200b02 	vstmdb	r0!, {d0}
0+07c <[^>]*> eeb80bc0 	fsitod	d0, s0
0+080 <[^>]*> eeb80b40 	fuitod	d0, s0
0+084 <[^>]*> eebd0b40 	ftosid	s0, d0
0+088 <[^>]*> eebd0bc0 	ftosizd	s0, d0
0+08c <[^>]*> eebc0b40 	ftouid	s0, d0
0+090 <[^>]*> eebc0bc0 	ftouizd	s0, d0
0+094 <[^>]*> eeb70ac0 	fcvtds	d0, s0
0+098 <[^>]*> eeb70bc0 	fcvtsd	s0, d0
0+09c <[^>]*> ee300b10 	vmov\.32	r0, d0\[1\]
0+0a0 <[^>]*> ee100b10 	vmov\.32	r0, d0\[0\]
0+0a4 <[^>]*> ee200b10 	vmov\.32	d0\[1\], r0
0+0a8 <[^>]*> ee000b10 	vmov\.32	d0\[0\], r0
0+0ac <[^>]*> eeb51b40 	fcmpzd	d1
0+0b0 <[^>]*> eeb52b40 	fcmpzd	d2
0+0b4 <[^>]*> eeb5fb40 	fcmpzd	d15
0+0b8 <[^>]*> eeb40b41 	fcmpd	d0, d1
0+0bc <[^>]*> eeb40b42 	fcmpd	d0, d2
0+0c0 <[^>]*> eeb40b4f 	fcmpd	d0, d15
0+0c4 <[^>]*> eeb41b40 	fcmpd	d1, d0
0+0c8 <[^>]*> eeb42b40 	fcmpd	d2, d0
0+0cc <[^>]*> eeb4fb40 	fcmpd	d15, d0
0+0d0 <[^>]*> eeb45b4c 	fcmpd	d5, d12
0+0d4 <[^>]*> eeb10b41 	fnegd	d0, d1
0+0d8 <[^>]*> eeb10b42 	fnegd	d0, d2
0+0dc <[^>]*> eeb10b4f 	fnegd	d0, d15
0+0e0 <[^>]*> eeb11b40 	fnegd	d1, d0
0+0e4 <[^>]*> eeb12b40 	fnegd	d2, d0
0+0e8 <[^>]*> eeb1fb40 	fnegd	d15, d0
0+0ec <[^>]*> eeb1cb45 	fnegd	d12, d5
0+0f0 <[^>]*> ee300b01 	faddd	d0, d0, d1
0+0f4 <[^>]*> ee300b02 	faddd	d0, d0, d2
0+0f8 <[^>]*> ee300b0f 	faddd	d0, d0, d15
0+0fc <[^>]*> ee310b00 	faddd	d0, d1, d0
0+100 <[^>]*> ee320b00 	faddd	d0, d2, d0
0+104 <[^>]*> ee3f0b00 	faddd	d0, d15, d0
0+108 <[^>]*> ee301b00 	faddd	d1, d0, d0
0+10c <[^>]*> ee302b00 	faddd	d2, d0, d0
0+110 <[^>]*> ee30fb00 	faddd	d15, d0, d0
0+114 <[^>]*> ee39cb05 	faddd	d12, d9, d5
0+118 <[^>]*> eeb70ae0 	fcvtds	d0, s1
0+11c <[^>]*> eeb70ac1 	fcvtds	d0, s2
0+120 <[^>]*> eeb70aef 	fcvtds	d0, s31
0+124 <[^>]*> eeb71ac0 	fcvtds	d1, s0
0+128 <[^>]*> eeb72ac0 	fcvtds	d2, s0
0+12c <[^>]*> eeb7fac0 	fcvtds	d15, s0
0+130 <[^>]*> eef70bc0 	fcvtsd	s1, d0
0+134 <[^>]*> eeb71bc0 	fcvtsd	s2, d0
0+138 <[^>]*> eef7fbc0 	fcvtsd	s31, d0
0+13c <[^>]*> eeb70bc1 	fcvtsd	s0, d1
0+140 <[^>]*> eeb70bc2 	fcvtsd	s0, d2
0+144 <[^>]*> eeb70bcf 	fcvtsd	s0, d15
0+148 <[^>]*> ee301b10 	vmov\.32	r1, d0\[1\]
0+14c <[^>]*> ee30eb10 	vmov\.32	lr, d0\[1\]
0+150 <[^>]*> ee310b10 	vmov\.32	r0, d1\[1\]
0+154 <[^>]*> ee320b10 	vmov\.32	r0, d2\[1\]
0+158 <[^>]*> ee3f0b10 	vmov\.32	r0, d15\[1\]
0+15c <[^>]*> ee101b10 	vmov\.32	r1, d0\[0\]
0+160 <[^>]*> ee10eb10 	vmov\.32	lr, d0\[0\]
0+164 <[^>]*> ee110b10 	vmov\.32	r0, d1\[0\]
0+168 <[^>]*> ee120b10 	vmov\.32	r0, d2\[0\]
0+16c <[^>]*> ee1f0b10 	vmov\.32	r0, d15\[0\]
0+170 <[^>]*> ee201b10 	vmov\.32	d0\[1\], r1
0+174 <[^>]*> ee20eb10 	vmov\.32	d0\[1\], lr
0+178 <[^>]*> ee210b10 	vmov\.32	d1\[1\], r0
0+17c <[^>]*> ee220b10 	vmov\.32	d2\[1\], r0
0+180 <[^>]*> ee2f0b10 	vmov\.32	d15\[1\], r0
0+184 <[^>]*> ee001b10 	vmov\.32	d0\[0\], r1
0+188 <[^>]*> ee00eb10 	vmov\.32	d0\[0\], lr
0+18c <[^>]*> ee010b10 	vmov\.32	d1\[0\], r0
0+190 <[^>]*> ee020b10 	vmov\.32	d2\[0\], r0
0+194 <[^>]*> ee0f0b10 	vmov\.32	d15\[0\], r0
0+198 <[^>]*> ed910b00 	vldr	d0, \[r1\]
0+19c <[^>]*> ed9e0b00 	vldr	d0, \[lr\]
0+1a0 <[^>]*> ed900b00 	vldr	d0, \[r0\]
0+1a4 <[^>]*> ed900bff 	vldr	d0, \[r0, #1020\]
0+1a8 <[^>]*> ed100bff 	vldr	d0, \[r0, #-1020\]
0+1ac <[^>]*> ed901b00 	vldr	d1, \[r0\]
0+1b0 <[^>]*> ed902b00 	vldr	d2, \[r0\]
0+1b4 <[^>]*> ed90fb00 	vldr	d15, \[r0\]
0+1b8 <[^>]*> ed8ccbc9 	vstr	d12, \[ip, #804\]
0+1bc <[^>]*> ec901b02 	vldmia	r0, {d1}
0+1c0 <[^>]*> ec902b02 	vldmia	r0, {d2}
0+1c4 <[^>]*> ec90fb02 	vldmia	r0, {d15}
0+1c8 <[^>]*> ec900b04 	vldmia	r0, {d0-d1}
0+1cc <[^>]*> ec900b06 	vldmia	r0, {d0-d2}
0+1d0 <[^>]*> ec900b20 	vldmia	r0, {d0-d15}
0+1d4 <[^>]*> ec901b1e 	vldmia	r0, {d1-d15}
0+1d8 <[^>]*> ec902b1c 	vldmia	r0, {d2-d15}
0+1dc <[^>]*> ec90eb04 	vldmia	r0, {d14-d15}
0+1e0 <[^>]*> ec910b02 	vldmia	r1, {d0}
0+1e4 <[^>]*> ec9e0b02 	vldmia	lr, {d0}
0+1e8 <[^>]*> eeb50b40 	fcmpzd	d0
0+1ec <[^>]*> eeb51b40 	fcmpzd	d1
0+1f0 <[^>]*> eeb52b40 	fcmpzd	d2
0+1f4 <[^>]*> eeb53b40 	fcmpzd	d3
0+1f8 <[^>]*> eeb54b40 	fcmpzd	d4
0+1fc <[^>]*> eeb55b40 	fcmpzd	d5
0+200 <[^>]*> eeb56b40 	fcmpzd	d6
0+204 <[^>]*> eeb57b40 	fcmpzd	d7
0+208 <[^>]*> eeb58b40 	fcmpzd	d8
0+20c <[^>]*> eeb59b40 	fcmpzd	d9
0+210 <[^>]*> eeb5ab40 	fcmpzd	d10
0+214 <[^>]*> eeb5bb40 	fcmpzd	d11
0+218 <[^>]*> eeb5cb40 	fcmpzd	d12
0+21c <[^>]*> eeb5db40 	fcmpzd	d13
0+220 <[^>]*> eeb5eb40 	fcmpzd	d14
0+224 <[^>]*> eeb5fb40 	fcmpzd	d15
0+228 <[^>]*> 0eb41bcf 	fcmpedeq	d1, d15
0+22c <[^>]*> 0eb52bc0 	fcmpezdeq	d2
0+230 <[^>]*> 0eb43b4e 	fcmpdeq	d3, d14
0+234 <[^>]*> 0eb54b40 	fcmpzdeq	d4
0+238 <[^>]*> 0eb05bcd 	fabsdeq	d5, d13
0+23c <[^>]*> 0eb06b4c 	fcpydeq	d6, d12
0+240 <[^>]*> 0eb17b4b 	fnegdeq	d7, d11
0+244 <[^>]*> 0eb18bca 	fsqrtdeq	d8, d10
0+248 <[^>]*> 0e319b0f 	fadddeq	d9, d1, d15
0+24c <[^>]*> 0e832b0e 	fdivdeq	d2, d3, d14
0+250 <[^>]*> 0e0d4b0c 	fmacdeq	d4, d13, d12
0+254 <[^>]*> 0e165b0b 	fmscdeq	d5, d6, d11
0+258 <[^>]*> 0e2a7b09 	fmuldeq	d7, d10, d9
0+25c <[^>]*> 0e098b4a 	fnmacdeq	d8, d9, d10
0+260 <[^>]*> 0e167b4b 	fnmscdeq	d7, d6, d11
0+264 <[^>]*> 0e245b4c 	fnmuldeq	d5, d4, d12
0+268 <[^>]*> 0e3d3b4e 	fsubdeq	d3, d13, d14
0+26c <[^>]*> 0d952b00 	vldreq	d2, \[r5\]
0+270 <[^>]*> 0d8c1b00 	vstreq	d1, \[ip\]
0+274 <[^>]*> 0c911b02 	vldmiaeq	r1, {d1}
0+278 <[^>]*> 0c922b02 	vldmiaeq	r2, {d2}
0+27c <[^>]*> 0cb33b02 	vldmiaeq	r3!, {d3}
0+280 <[^>]*> 0cb44b02 	vldmiaeq	r4!, {d4}
0+284 <[^>]*> 0d355b02 	vldmdbeq	r5!, {d5}
0+288 <[^>]*> 0d366b02 	vldmdbeq	r6!, {d6}
0+28c <[^>]*> 0c87fb02 	vstmiaeq	r7, {d15}
0+290 <[^>]*> 0c88eb02 	vstmiaeq	r8, {d14}
0+294 <[^>]*> 0ca9db02 	vstmiaeq	r9!, {d13}
0+298 <[^>]*> 0caacb02 	vstmiaeq	sl!, {d12}
0+29c <[^>]*> 0d2bbb02 	vstmdbeq	fp!, {d11}
0+2a0 <[^>]*> 0d2cab02 	vstmdbeq	ip!, {d10}
0+2a4 <[^>]*> 0eb8fbe0 	fsitodeq	d15, s1
0+2a8 <[^>]*> 0eb81b6f 	fuitodeq	d1, s31
0+2ac <[^>]*> 0efd0b4f 	ftosideq	s1, d15
0+2b0 <[^>]*> 0efdfbc2 	ftosizdeq	s31, d2
0+2b4 <[^>]*> 0efc7b42 	ftouideq	s15, d2
0+2b8 <[^>]*> 0efc5bc3 	ftouizdeq	s11, d3
0+2bc <[^>]*> 0eb71ac5 	fcvtdseq	d1, s10
0+2c0 <[^>]*> 0ef75bc1 	fcvtsdeq	s11, d1
0+2c4 <[^>]*> 0e318b10 	vmoveq\.32	r8, d1\[1\]
0+2c8 <[^>]*> 0e1f7b10 	vmoveq\.32	r7, d15\[0\]
0+2cc <[^>]*> 0e21fb10 	vmoveq\.32	d1\[1\], pc
0+2d0 <[^>]*> 0e0f1b10 	vmoveq\.32	d15\[0\], r1
0+2d4 <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
0+2d8 <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
0+2dc <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
