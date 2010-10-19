#name: THUMB V6K instructions
#as: -march=armv6k -mthumb
#objdump: -dr --prefix-addresses --show-raw-insn -M force-thumb

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]*> bf10 *	yield
0+002 <[^>]*> bf20 *	wfe
0+004 <[^>]*> bf30 *	wfi
0+006 <[^>]*> bf40 *	sev
0+008 <[^>]*> 46c0 *	nop[ \t]+\(mov r8, r8\)
0+00a <[^>]*> 46c0 *	nop[ \t]+\(mov r8, r8\)
0+00c <[^>]*> 46c0 *	nop[ \t]+\(mov r8, r8\)
0+00e <[^>]*> 46c0 *	nop[ \t]+\(mov r8, r8\)
