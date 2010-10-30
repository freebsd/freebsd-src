# name: VFPv3 extra D registers
# as: -mfpu=vfp3
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section \.text:
0[0-9a-f]+ <[^>]+> eeb03b66 	fcpyd	d3, d22
0[0-9a-f]+ <[^>]+> eef06b43 	fcpyd	d22, d3
0[0-9a-f]+ <[^>]+> eef76acb 	fcvtds	d22, s22
0[0-9a-f]+ <[^>]+> eeb7bbe6 	fcvtsd	s22, d22
0[0-9a-f]+ <[^>]+> ee254b90 	vmov\.32	d21\[1\], r4
0[0-9a-f]+ <[^>]+> ee0b5b90 	vmov\.32	d27\[0\], r5
0[0-9a-f]+ <[^>]+> ee376b90 	vmov\.32	r6, d23\[1\]
0[0-9a-f]+ <[^>]+> ee197b90 	vmov\.32	r7, d25\[0\]
0[0-9a-f]+ <[^>]+> eef86bcb 	fsitod	d22, s22
0[0-9a-f]+ <[^>]+> eef85b6a 	fuitod	d21, s21
0[0-9a-f]+ <[^>]+> eebdab64 	ftosid	s20, d20
0[0-9a-f]+ <[^>]+> eebdabe4 	ftosizd	s20, d20
0[0-9a-f]+ <[^>]+> eefc9b63 	ftouid	s19, d19
0[0-9a-f]+ <[^>]+> eefc9be3 	ftouizd	s19, d19
0[0-9a-f]+ <[^>]+> edda3b01 	vldr	d19, \[sl, #4\]
0[0-9a-f]+ <[^>]+> edca5b01 	vstr	d21, \[sl, #4\]
0[0-9a-f]+ <[^>]+> ecba5b04 	vldmia	sl!, {d5-d6}
0[0-9a-f]+ <[^>]+> ecfa2b06 	vldmia	sl!, {d18-d20}
0[0-9a-f]+ <[^>]+> ecba5b05 	fldmiax	sl!, {d5-d6}
0[0-9a-f]+ <[^>]+> ecfa2b07 	fldmiax	sl!, {d18-d20}
0[0-9a-f]+ <[^>]+> ed7a2b05 	fldmdbx	sl!, {d18-d19}
0[0-9a-f]+ <[^>]+> ecc94b0a 	vstmia	r9, {d20-d24}
0[0-9a-f]+ <[^>]+> eeb03bc5 	fabsd	d3, d5
0[0-9a-f]+ <[^>]+> eeb0cbe2 	fabsd	d12, d18
0[0-9a-f]+ <[^>]+> eef02be3 	fabsd	d18, d19
0[0-9a-f]+ <[^>]+> eeb13b45 	fnegd	d3, d5
0[0-9a-f]+ <[^>]+> eeb1cb62 	fnegd	d12, d18
0[0-9a-f]+ <[^>]+> eef12b63 	fnegd	d18, d19
0[0-9a-f]+ <[^>]+> eeb13bc5 	fsqrtd	d3, d5
0[0-9a-f]+ <[^>]+> eeb1cbe2 	fsqrtd	d12, d18
0[0-9a-f]+ <[^>]+> eef12be3 	fsqrtd	d18, d19
0[0-9a-f]+ <[^>]+> ee353b06 	faddd	d3, d5, d6
0[0-9a-f]+ <[^>]+> ee32cb84 	faddd	d12, d18, d4
0[0-9a-f]+ <[^>]+> ee732ba4 	faddd	d18, d19, d20
0[0-9a-f]+ <[^>]+> ee353b46 	fsubd	d3, d5, d6
0[0-9a-f]+ <[^>]+> ee32cbc4 	fsubd	d12, d18, d4
0[0-9a-f]+ <[^>]+> ee732be4 	fsubd	d18, d19, d20
0[0-9a-f]+ <[^>]+> ee253b06 	fmuld	d3, d5, d6
0[0-9a-f]+ <[^>]+> ee22cb84 	fmuld	d12, d18, d4
0[0-9a-f]+ <[^>]+> ee632ba4 	fmuld	d18, d19, d20
0[0-9a-f]+ <[^>]+> ee853b06 	fdivd	d3, d5, d6
0[0-9a-f]+ <[^>]+> ee82cb84 	fdivd	d12, d18, d4
0[0-9a-f]+ <[^>]+> eec32ba4 	fdivd	d18, d19, d20
0[0-9a-f]+ <[^>]+> ee053b06 	fmacd	d3, d5, d6
0[0-9a-f]+ <[^>]+> ee02cb84 	fmacd	d12, d18, d4
0[0-9a-f]+ <[^>]+> ee432ba4 	fmacd	d18, d19, d20
0[0-9a-f]+ <[^>]+> ee153b06 	fmscd	d3, d5, d6
0[0-9a-f]+ <[^>]+> ee12cb84 	fmscd	d12, d18, d4
0[0-9a-f]+ <[^>]+> ee532ba4 	fmscd	d18, d19, d20
0[0-9a-f]+ <[^>]+> ee253b46 	fnmuld	d3, d5, d6
0[0-9a-f]+ <[^>]+> ee22cbc4 	fnmuld	d12, d18, d4
0[0-9a-f]+ <[^>]+> ee632be4 	fnmuld	d18, d19, d20
0[0-9a-f]+ <[^>]+> ee053b46 	fnmacd	d3, d5, d6
0[0-9a-f]+ <[^>]+> ee02cbc4 	fnmacd	d12, d18, d4
0[0-9a-f]+ <[^>]+> ee432be4 	fnmacd	d18, d19, d20
0[0-9a-f]+ <[^>]+> ee153b46 	fnmscd	d3, d5, d6
0[0-9a-f]+ <[^>]+> ee12cbc4 	fnmscd	d12, d18, d4
0[0-9a-f]+ <[^>]+> ee532be4 	fnmscd	d18, d19, d20
0[0-9a-f]+ <[^>]+> eeb43b62 	fcmpd	d3, d18
0[0-9a-f]+ <[^>]+> eef42b43 	fcmpd	d18, d3
0[0-9a-f]+ <[^>]+> eef53b40 	fcmpzd	d19
0[0-9a-f]+ <[^>]+> eeb43be2 	fcmped	d3, d18
0[0-9a-f]+ <[^>]+> eef42bc3 	fcmped	d18, d3
0[0-9a-f]+ <[^>]+> eef53bc0 	fcmpezd	d19
0[0-9a-f]+ <[^>]+> ec443b3f 	vmov	d31, r3, r4
0[0-9a-f]+ <[^>]+> ec565b3e 	vmov	r5, r6, d30
