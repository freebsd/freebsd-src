#objdump: -dr --prefix-addresses --show-raw-insn
#name: VFP Additional instructions
#as: -mfpu=vfp

# Test the ARM VFP Double Precision instructions

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]*> ec4a5b10 	vmov	d0, r5, sl
0+004 <[^>]*> ec5a5b10 	vmov	r5, sl, d0
0+008 <[^>]*> ec4a5a37 	fmsrr	{s15, s16}, r5, sl
0+00c <[^>]*> ec5a5a37 	fmrrs	r5, sl, {s15, s16}
0+010 <[^>]*> ec45ab1f 	vmov	d15, sl, r5
0+014 <[^>]*> ec55ab1f 	vmov	sl, r5, d15
0+018 <[^>]*> ec45aa38 	fmsrr	{s17, s18}, sl, r5
0+01c <[^>]*> ec55aa38 	fmrrs	sl, r5, {s17, s18}
