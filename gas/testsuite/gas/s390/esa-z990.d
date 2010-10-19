#name: s390 opcode
#objdump: -drw

.*: +file format .*

Disassembly of section .text:

.* <foo>:
.*:	b9 2e 00 69 [	 ]*km	%r6,%r9
.*:	b9 2f 00 69 [	 ]*kmc	%r6,%r9
.*:	b9 3e 00 69 [	 ]*kimd	%r6,%r9
.*:	b9 3f 00 69 [	 ]*klmd	%r6,%r9
.*:	b9 1e 00 69 [	 ]*kmac	%r6,%r9
