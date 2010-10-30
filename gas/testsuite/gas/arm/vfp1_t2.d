#objdump: -dr --prefix-addresses --show-raw-insn
#name: Thumb-2 VFP Double-precision instructions
#as: -mfpu=vfp

# Test the ARM VFP Double Precision instructions

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]*> eeb4 0bc0 	fcmped	d0, d0
0+004 <[^>]*> eeb5 0bc0 	fcmpezd	d0
0+008 <[^>]*> eeb4 0b40 	fcmpd	d0, d0
0+00c <[^>]*> eeb5 0b40 	fcmpzd	d0
0+010 <[^>]*> eeb0 0bc0 	fabsd	d0, d0
0+014 <[^>]*> eeb0 0b40 	fcpyd	d0, d0
0+018 <[^>]*> eeb1 0b40 	fnegd	d0, d0
0+01c <[^>]*> eeb1 0bc0 	fsqrtd	d0, d0
0+020 <[^>]*> ee30 0b00 	faddd	d0, d0, d0
0+024 <[^>]*> ee80 0b00 	fdivd	d0, d0, d0
0+028 <[^>]*> ee00 0b00 	fmacd	d0, d0, d0
0+02c <[^>]*> ee10 0b00 	fmscd	d0, d0, d0
0+030 <[^>]*> ee20 0b00 	fmuld	d0, d0, d0
0+034 <[^>]*> ee00 0b40 	fnmacd	d0, d0, d0
0+038 <[^>]*> ee10 0b40 	fnmscd	d0, d0, d0
0+03c <[^>]*> ee20 0b40 	fnmuld	d0, d0, d0
0+040 <[^>]*> ee30 0b40 	fsubd	d0, d0, d0
0+044 <[^>]*> ed90 0b00 	vldr	d0, \[r0\]
0+048 <[^>]*> ed80 0b00 	vstr	d0, \[r0\]
0+04c <[^>]*> ec90 0b02 	vldmia	r0, {d0}
0+050 <[^>]*> ec90 0b02 	vldmia	r0, {d0}
0+054 <[^>]*> ecb0 0b02 	vldmia	r0!, {d0}
0+058 <[^>]*> ecb0 0b02 	vldmia	r0!, {d0}
0+05c <[^>]*> ed30 0b02 	vldmdb	r0!, {d0}
0+060 <[^>]*> ed30 0b02 	vldmdb	r0!, {d0}
0+064 <[^>]*> ec80 0b02 	vstmia	r0, {d0}
0+068 <[^>]*> ec80 0b02 	vstmia	r0, {d0}
0+06c <[^>]*> eca0 0b02 	vstmia	r0!, {d0}
0+070 <[^>]*> eca0 0b02 	vstmia	r0!, {d0}
0+074 <[^>]*> ed20 0b02 	vstmdb	r0!, {d0}
0+078 <[^>]*> ed20 0b02 	vstmdb	r0!, {d0}
0+07c <[^>]*> eeb8 0bc0 	fsitod	d0, s0
0+080 <[^>]*> eeb8 0b40 	fuitod	d0, s0
0+084 <[^>]*> eebd 0b40 	ftosid	s0, d0
0+088 <[^>]*> eebd 0bc0 	ftosizd	s0, d0
0+08c <[^>]*> eebc 0b40 	ftouid	s0, d0
0+090 <[^>]*> eebc 0bc0 	ftouizd	s0, d0
0+094 <[^>]*> eeb7 0ac0 	fcvtds	d0, s0
0+098 <[^>]*> eeb7 0bc0 	fcvtsd	s0, d0
0+09c <[^>]*> ee30 0b10 	vmov\.32	r0, d0\[1\]
0+0a0 <[^>]*> ee10 0b10 	vmov\.32	r0, d0\[0\]
0+0a4 <[^>]*> ee20 0b10 	vmov\.32	d0\[1\], r0
0+0a8 <[^>]*> ee00 0b10 	vmov\.32	d0\[0\], r0
0+0ac <[^>]*> eeb5 1b40 	fcmpzd	d1
0+0b0 <[^>]*> eeb5 2b40 	fcmpzd	d2
0+0b4 <[^>]*> eeb5 fb40 	fcmpzd	d15
0+0b8 <[^>]*> eeb4 0b41 	fcmpd	d0, d1
0+0bc <[^>]*> eeb4 0b42 	fcmpd	d0, d2
0+0c0 <[^>]*> eeb4 0b4f 	fcmpd	d0, d15
0+0c4 <[^>]*> eeb4 1b40 	fcmpd	d1, d0
0+0c8 <[^>]*> eeb4 2b40 	fcmpd	d2, d0
0+0cc <[^>]*> eeb4 fb40 	fcmpd	d15, d0
0+0d0 <[^>]*> eeb4 5b4c 	fcmpd	d5, d12
0+0d4 <[^>]*> eeb1 0b41 	fnegd	d0, d1
0+0d8 <[^>]*> eeb1 0b42 	fnegd	d0, d2
0+0dc <[^>]*> eeb1 0b4f 	fnegd	d0, d15
0+0e0 <[^>]*> eeb1 1b40 	fnegd	d1, d0
0+0e4 <[^>]*> eeb1 2b40 	fnegd	d2, d0
0+0e8 <[^>]*> eeb1 fb40 	fnegd	d15, d0
0+0ec <[^>]*> eeb1 cb45 	fnegd	d12, d5
0+0f0 <[^>]*> ee30 0b01 	faddd	d0, d0, d1
0+0f4 <[^>]*> ee30 0b02 	faddd	d0, d0, d2
0+0f8 <[^>]*> ee30 0b0f 	faddd	d0, d0, d15
0+0fc <[^>]*> ee31 0b00 	faddd	d0, d1, d0
0+100 <[^>]*> ee32 0b00 	faddd	d0, d2, d0
0+104 <[^>]*> ee3f 0b00 	faddd	d0, d15, d0
0+108 <[^>]*> ee30 1b00 	faddd	d1, d0, d0
0+10c <[^>]*> ee30 2b00 	faddd	d2, d0, d0
0+110 <[^>]*> ee30 fb00 	faddd	d15, d0, d0
0+114 <[^>]*> ee39 cb05 	faddd	d12, d9, d5
0+118 <[^>]*> eeb7 0ae0 	fcvtds	d0, s1
0+11c <[^>]*> eeb7 0ac1 	fcvtds	d0, s2
0+120 <[^>]*> eeb7 0aef 	fcvtds	d0, s31
0+124 <[^>]*> eeb7 1ac0 	fcvtds	d1, s0
0+128 <[^>]*> eeb7 2ac0 	fcvtds	d2, s0
0+12c <[^>]*> eeb7 fac0 	fcvtds	d15, s0
0+130 <[^>]*> eef7 0bc0 	fcvtsd	s1, d0
0+134 <[^>]*> eeb7 1bc0 	fcvtsd	s2, d0
0+138 <[^>]*> eef7 fbc0 	fcvtsd	s31, d0
0+13c <[^>]*> eeb7 0bc1 	fcvtsd	s0, d1
0+140 <[^>]*> eeb7 0bc2 	fcvtsd	s0, d2
0+144 <[^>]*> eeb7 0bcf 	fcvtsd	s0, d15
0+148 <[^>]*> ee30 1b10 	vmov\.32	r1, d0\[1\]
0+14c <[^>]*> ee30 eb10 	vmov\.32	lr, d0\[1\]
0+150 <[^>]*> ee31 0b10 	vmov\.32	r0, d1\[1\]
0+154 <[^>]*> ee32 0b10 	vmov\.32	r0, d2\[1\]
0+158 <[^>]*> ee3f 0b10 	vmov\.32	r0, d15\[1\]
0+15c <[^>]*> ee10 1b10 	vmov\.32	r1, d0\[0\]
0+160 <[^>]*> ee10 eb10 	vmov\.32	lr, d0\[0\]
0+164 <[^>]*> ee11 0b10 	vmov\.32	r0, d1\[0\]
0+168 <[^>]*> ee12 0b10 	vmov\.32	r0, d2\[0\]
0+16c <[^>]*> ee1f 0b10 	vmov\.32	r0, d15\[0\]
0+170 <[^>]*> ee20 1b10 	vmov\.32	d0\[1\], r1
0+174 <[^>]*> ee20 eb10 	vmov\.32	d0\[1\], lr
0+178 <[^>]*> ee21 0b10 	vmov\.32	d1\[1\], r0
0+17c <[^>]*> ee22 0b10 	vmov\.32	d2\[1\], r0
0+180 <[^>]*> ee2f 0b10 	vmov\.32	d15\[1\], r0
0+184 <[^>]*> ee00 1b10 	vmov\.32	d0\[0\], r1
0+188 <[^>]*> ee00 eb10 	vmov\.32	d0\[0\], lr
0+18c <[^>]*> ee01 0b10 	vmov\.32	d1\[0\], r0
0+190 <[^>]*> ee02 0b10 	vmov\.32	d2\[0\], r0
0+194 <[^>]*> ee0f 0b10 	vmov\.32	d15\[0\], r0
0+198 <[^>]*> ed91 0b00 	vldr	d0, \[r1\]
0+19c <[^>]*> ed9e 0b00 	vldr	d0, \[lr\]
0+1a0 <[^>]*> ed90 0b00 	vldr	d0, \[r0\]
0+1a4 <[^>]*> ed90 0bff 	vldr	d0, \[r0, #1020\]
0+1a8 <[^>]*> ed10 0bff 	vldr	d0, \[r0, #-1020\]
0+1ac <[^>]*> ed90 1b00 	vldr	d1, \[r0\]
0+1b0 <[^>]*> ed90 2b00 	vldr	d2, \[r0\]
0+1b4 <[^>]*> ed90 fb00 	vldr	d15, \[r0\]
0+1b8 <[^>]*> ed8c cbc9 	vstr	d12, \[ip, #804\]
0+1bc <[^>]*> ec90 1b02 	vldmia	r0, {d1}
0+1c0 <[^>]*> ec90 2b02 	vldmia	r0, {d2}
0+1c4 <[^>]*> ec90 fb02 	vldmia	r0, {d15}
0+1c8 <[^>]*> ec90 0b04 	vldmia	r0, {d0-d1}
0+1cc <[^>]*> ec90 0b06 	vldmia	r0, {d0-d2}
0+1d0 <[^>]*> ec90 0b20 	vldmia	r0, {d0-d15}
0+1d4 <[^>]*> ec90 1b1e 	vldmia	r0, {d1-d15}
0+1d8 <[^>]*> ec90 2b1c 	vldmia	r0, {d2-d15}
0+1dc <[^>]*> ec90 eb04 	vldmia	r0, {d14-d15}
0+1e0 <[^>]*> ec91 0b02 	vldmia	r1, {d0}
0+1e4 <[^>]*> ec9e 0b02 	vldmia	lr, {d0}
0+1e8 <[^>]*> eeb5 0b40 	fcmpzd	d0
0+1ec <[^>]*> eeb5 1b40 	fcmpzd	d1
0+1f0 <[^>]*> eeb5 2b40 	fcmpzd	d2
0+1f4 <[^>]*> eeb5 3b40 	fcmpzd	d3
0+1f8 <[^>]*> eeb5 4b40 	fcmpzd	d4
0+1fc <[^>]*> eeb5 5b40 	fcmpzd	d5
0+200 <[^>]*> eeb5 6b40 	fcmpzd	d6
0+204 <[^>]*> eeb5 7b40 	fcmpzd	d7
0+208 <[^>]*> eeb5 8b40 	fcmpzd	d8
0+20c <[^>]*> eeb5 9b40 	fcmpzd	d9
0+210 <[^>]*> eeb5 ab40 	fcmpzd	d10
0+214 <[^>]*> eeb5 bb40 	fcmpzd	d11
0+218 <[^>]*> eeb5 cb40 	fcmpzd	d12
0+21c <[^>]*> eeb5 db40 	fcmpzd	d13
0+220 <[^>]*> eeb5 eb40 	fcmpzd	d14
0+224 <[^>]*> eeb5 fb40 	fcmpzd	d15
0+228 <[^>]*> bf01      	itttt	eq
0+22a <[^>]*> eeb4 1bcf 	fcmpedeq	d1, d15
0+22e <[^>]*> eeb5 2bc0 	fcmpezdeq	d2
0+232 <[^>]*> eeb4 3b4e 	fcmpdeq	d3, d14
0+236 <[^>]*> eeb5 4b40 	fcmpzdeq	d4
0+23a <[^>]*> bf01      	itttt	eq
0+23c <[^>]*> eeb0 5bcd 	fabsdeq	d5, d13
0+240 <[^>]*> eeb0 6b4c 	fcpydeq	d6, d12
0+244 <[^>]*> eeb1 7b4b 	fnegdeq	d7, d11
0+248 <[^>]*> eeb1 8bca 	fsqrtdeq	d8, d10
0+24c <[^>]*> bf01      	itttt	eq
0+24e <[^>]*> ee31 9b0f 	fadddeq	d9, d1, d15
0+252 <[^>]*> ee83 2b0e 	fdivdeq	d2, d3, d14
0+256 <[^>]*> ee0d 4b0c 	fmacdeq	d4, d13, d12
0+25a <[^>]*> ee16 5b0b 	fmscdeq	d5, d6, d11
0+25e <[^>]*> bf01      	itttt	eq
0+260 <[^>]*> ee2a 7b09 	fmuldeq	d7, d10, d9
0+264 <[^>]*> ee09 8b4a 	fnmacdeq	d8, d9, d10
0+268 <[^>]*> ee16 7b4b 	fnmscdeq	d7, d6, d11
0+26c <[^>]*> ee24 5b4c 	fnmuldeq	d5, d4, d12
0+270 <[^>]*> bf02      	ittt	eq
0+272 <[^>]*> ee3d 3b4e 	fsubdeq	d3, d13, d14
0+276 <[^>]*> ed95 2b00 	vldreq	d2, \[r5\]
0+27a <[^>]*> ed8c 1b00 	vstreq	d1, \[ip\]
0+27e <[^>]*> bf01      	itttt	eq
0+280 <[^>]*> ec91 1b02 	vldmiaeq	r1, {d1}
0+284 <[^>]*> ec92 2b02 	vldmiaeq	r2, {d2}
0+288 <[^>]*> ecb3 3b02 	vldmiaeq	r3!, {d3}
0+28c <[^>]*> ecb4 4b02 	vldmiaeq	r4!, {d4}
0+290 <[^>]*> bf01      	itttt	eq
0+292 <[^>]*> ed35 5b02 	vldmdbeq	r5!, {d5}
0+296 <[^>]*> ed36 6b02 	vldmdbeq	r6!, {d6}
0+29a <[^>]*> ec87 fb02 	vstmiaeq	r7, {d15}
0+29e <[^>]*> ec88 eb02 	vstmiaeq	r8, {d14}
0+2a2 <[^>]*> bf01      	itttt	eq
0+2a4 <[^>]*> eca9 db02 	vstmiaeq	r9!, {d13}
0+2a8 <[^>]*> ecaa cb02 	vstmiaeq	sl!, {d12}
0+2ac <[^>]*> ed2b bb02 	vstmdbeq	fp!, {d11}
0+2b0 <[^>]*> ed2c ab02 	vstmdbeq	ip!, {d10}
0+2b4 <[^>]*> bf01      	itttt	eq
0+2b6 <[^>]*> eeb8 fbe0 	fsitodeq	d15, s1
0+2ba <[^>]*> eeb8 1b6f 	fuitodeq	d1, s31
0+2be <[^>]*> eefd 0b4f 	ftosideq	s1, d15
0+2c2 <[^>]*> eefd fbc2 	ftosizdeq	s31, d2
0+2c6 <[^>]*> bf01      	itttt	eq
0+2c8 <[^>]*> eefc 7b42 	ftouideq	s15, d2
0+2cc <[^>]*> eefc 5bc3 	ftouizdeq	s11, d3
0+2d0 <[^>]*> eeb7 1ac5 	fcvtdseq	d1, s10
0+2d4 <[^>]*> eef7 5bc1 	fcvtsdeq	s11, d1
0+2d8 <[^>]*> bf01      	itttt	eq
0+2da <[^>]*> ee31 8b10 	vmoveq\.32	r8, d1\[1\]
0+2de <[^>]*> ee1f 7b10 	vmoveq\.32	r7, d15\[0\]
0+2e2 <[^>]*> ee21 fb10 	vmoveq\.32	d1\[1\], pc
0+2e6 <[^>]*> ee0f 1b10 	vmoveq\.32	d15\[0\], r1
0+2ea <[^>]*> bf00      	nop
0+2ec <[^>]*> bf00      	nop
0+2ee <[^>]*> bf00      	nop
