#name: Local BLX instructions
#objdump: -dr --prefix-addresses --show-raw-insn
#skip: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*
#as:

# Test assembler resolution of blx instructions.

.*: +file format .*arm.*

Disassembly of section .text:

0+00 <[^>]*> fa000000 	blx	00+8 <foo>
0+04 <[^>]*> fbffffff 	blx	00+a <foo2>
0+08 <[^>]*> 46c0      	nop			\(mov r8, r8\)
0+0a <[^>]*> 46c0      	nop			\(mov r8, r8\)
