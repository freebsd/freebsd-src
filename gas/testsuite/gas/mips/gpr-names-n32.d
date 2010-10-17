#objdump: -dr --prefix-addresses --show-raw-insn -M gpr-names=n32
#name: MIPS GPR disassembly (n32)
#source: gpr-names.s

# Check objdump's handling of -M gpr-names=foo options.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> 3c000000 	lui	zero,0x0
0+0004 <[^>]*> 3c010000 	lui	at,0x0
0+0008 <[^>]*> 3c020000 	lui	v0,0x0
0+000c <[^>]*> 3c030000 	lui	v1,0x0
0+0010 <[^>]*> 3c040000 	lui	a0,0x0
0+0014 <[^>]*> 3c050000 	lui	a1,0x0
0+0018 <[^>]*> 3c060000 	lui	a2,0x0
0+001c <[^>]*> 3c070000 	lui	a3,0x0
0+0020 <[^>]*> 3c080000 	lui	a4,0x0
0+0024 <[^>]*> 3c090000 	lui	a5,0x0
0+0028 <[^>]*> 3c0a0000 	lui	a6,0x0
0+002c <[^>]*> 3c0b0000 	lui	a7,0x0
0+0030 <[^>]*> 3c0c0000 	lui	t0,0x0
0+0034 <[^>]*> 3c0d0000 	lui	t1,0x0
0+0038 <[^>]*> 3c0e0000 	lui	t2,0x0
0+003c <[^>]*> 3c0f0000 	lui	t3,0x0
0+0040 <[^>]*> 3c100000 	lui	s0,0x0
0+0044 <[^>]*> 3c110000 	lui	s1,0x0
0+0048 <[^>]*> 3c120000 	lui	s2,0x0
0+004c <[^>]*> 3c130000 	lui	s3,0x0
0+0050 <[^>]*> 3c140000 	lui	s4,0x0
0+0054 <[^>]*> 3c150000 	lui	s5,0x0
0+0058 <[^>]*> 3c160000 	lui	s6,0x0
0+005c <[^>]*> 3c170000 	lui	s7,0x0
0+0060 <[^>]*> 3c180000 	lui	t8,0x0
0+0064 <[^>]*> 3c190000 	lui	t9,0x0
0+0068 <[^>]*> 3c1a0000 	lui	k0,0x0
0+006c <[^>]*> 3c1b0000 	lui	k1,0x0
0+0070 <[^>]*> 3c1c0000 	lui	gp,0x0
0+0074 <[^>]*> 3c1d0000 	lui	sp,0x0
0+0078 <[^>]*> 3c1e0000 	lui	s8,0x0
0+007c <[^>]*> 3c1f0000 	lui	ra,0x0
	\.\.\.
