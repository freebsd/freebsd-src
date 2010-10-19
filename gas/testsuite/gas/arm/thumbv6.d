#name: THUMB V6 instructions
#as: -march=armv6j -mthumb
#objdump: -dr --prefix-addresses --show-raw-insn -M force-thumb

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]*> b666 *	cpsie	ai
0+002 <[^>]*> b675 *	cpsid	af
0+004 <[^>]*> 4623 *	mov	r3, r4
0+006 <[^>]*> ba3a *	rev	r2, r7
0+008 <[^>]*> ba4d *	rev16	r5, r1
0+00a <[^>]*> baf3 *	revsh	r3, r6
0+00c <[^>]*> b658 *	setend	be
0+00e <[^>]*> b650 *	setend	le
0+010 <[^>]*> b208 *	sxth	r0, r1
0+012 <[^>]*> b251 *	sxtb	r1, r2
0+014 <[^>]*> b2a3 *	uxth	r3, r4
0+016 <[^>]*> b2f5 *	uxtb	r5, r6
0+018 <[^>]*> 46c0 *	nop[ 	]+\(mov r8, r8\)
0+01a <[^>]*> 46c0 *	nop[ 	]+\(mov r8, r8\)
0+01c <[^>]*> 46c0 *	nop[ 	]+\(mov r8, r8\)
0+01e <[^>]*> 46c0 *	nop[ 	]+\(mov r8, r8\)
