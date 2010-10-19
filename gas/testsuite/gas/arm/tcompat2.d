#name: Thumb ARM-compat pseudos
#objdump: -dr --prefix-addresses --show-raw-insn -M force-thumb
#as:

# Test the Thumb pseudo instructions that exist for ARM source compatibility

.*: +file format .*arm.*

Disassembly of section .text:

0+00 <[^>]*> 4148 *	adcs	r0, r1
0+02 <[^>]*> 4148 *	adcs	r0, r1
0+04 <[^>]*> 4008 *	ands	r0, r1
0+06 <[^>]*> 4008 *	ands	r0, r1
0+08 <[^>]*> 4048 *	eors	r0, r1
0+0a <[^>]*> 4048 *	eors	r0, r1
0+0c <[^>]*> 4348 *	muls	r0, r1
0+0e <[^>]*> 4348 *	muls	r0, r1
0+10 <[^>]*> 4308 *	orrs	r0, r1
0+12 <[^>]*> 4308 *	orrs	r0, r1
0+14 <[^>]*> 4388 *	bics	r0, r1
0+16 <[^>]*> 4188 *	sbcs	r0, r1
0+18 <[^>]*> 46c0 *	nop			\(mov r8, r8\)
0+1a <[^>]*> 46c0 *	nop			\(mov r8, r8\)
0+1c <[^>]*> 46c0 *	nop			\(mov r8, r8\)
0+1e <[^>]*> 46c0 *	nop			\(mov r8, r8\)
