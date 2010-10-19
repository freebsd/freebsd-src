# as:
# objdump: -dr --prefix-addresses --show-raw-insn
# skip: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]+> bf18      	it	ne
0+002 <[^>]+> e7fd      	b(|ne).n	0+0 <[^>]+>
0+004 <[^>]+> bf38      	it	cc
0+006 <[^>]+> f7ff bffb 	b(|cc).w	0+0 <[^>]+>
0+00a <[^>]+> bf28      	it	cs
0+00c <[^>]+> f7ff fff8 	bl(|cs)	0+0 <[^>]+>
0+010 <[^>]+> bfb8      	it	lt
0+012 <[^>]+> 47a8      	blx(|lr)	r5
0+014 <[^>]+> bf08      	it	eq
0+016 <[^>]+> 4740      	bx(|eq)	r8
0+018 <[^>]+> bfc8      	it	gt
0+01a <[^>]+> e8d4 f001 	tbb(|gt)	\[r4, r1\]
0+01e <[^>]+> bfb8      	it	lt
0+020 <[^>]+> df00      	svc(|lt)	0
0+022 <[^>]+> bfdc      	itt	le
0+024 <[^>]+> be00      	bkpt	0x0000
0+026 <[^>]+> bf00      	nop
0+028 <[^>]+> bf00      	nop
0+02a <[^>]+> bf00      	nop
