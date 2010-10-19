# name: El Segundo instructions
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section \.text:
0+00 <[^>]+> c1003281 	smlabbgt	r0, r1, r2, r3
0+04 <[^>]+> e1003281 	smlabb	r0, r1, r2, r3
0+08 <[^>]+> e10032a1 	smlatb	r0, r1, r2, r3
0+0c <[^>]+> e10032c1 	smlabt	r0, r1, r2, r3
0+10 <[^>]+> e10032e1 	smlatt	r0, r1, r2, r3
0+14 <[^>]+> c1203281 	smlawbgt	r0, r1, r2, r3
0+18 <[^>]+> e1203281 	smlawb	r0, r1, r2, r3
0+1c <[^>]+> e12032c1 	smlawt	r0, r1, r2, r3
0+20 <[^>]+> c1410382 	smlalbbgt	r0, r1, r2, r3
0+24 <[^>]+> e1410382 	smlalbb	r0, r1, r2, r3
0+28 <[^>]+> e14103a2 	smlaltb	r0, r1, r2, r3
0+2c <[^>]+> e14103c2 	smlalbt	r0, r1, r2, r3
0+30 <[^>]+> e14103e2 	smlaltt	r0, r1, r2, r3
0+34 <[^>]+> c1600281 	smulbbgt	r0, r1, r2
0+38 <[^>]+> e1600281 	smulbb	r0, r1, r2
0+3c <[^>]+> e16002a1 	smultb	r0, r1, r2
0+40 <[^>]+> e16002c1 	smulbt	r0, r1, r2
0+44 <[^>]+> e16002e1 	smultt	r0, r1, r2
0+48 <[^>]+> c12002a1 	smulwbgt	r0, r1, r2
0+4c <[^>]+> e12002a1 	smulwb	r0, r1, r2
0+50 <[^>]+> e12002e1 	smulwt	r0, r1, r2
0+54 <[^>]+> c1020051 	qaddgt	r0, r1, r2
0+58 <[^>]+> e1020051 	qadd	r0, r1, r2
0+5c <[^>]+> e1420051 	qdadd	r0, r1, r2
0+60 <[^>]+> e1220051 	qsub	r0, r1, r2
0+64 <[^>]+> e1620051 	qdsub	r0, r1, r2
0+68 <[^>]+> e1220051 	qsub	r0, r1, r2
0+6c <[^>]+> e1a00000 	nop			\(mov r0,r0\)
