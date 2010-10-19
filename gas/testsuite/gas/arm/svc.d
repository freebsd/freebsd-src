# name: SWI/SVC instructions
# objdump: -dr --prefix-addresses --show-raw-insn
# skip: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*

.*: +file format .*arm.*

Disassembly of section \.text:
0+000 <[^>]+> ef123456 	(swi|svc)	0x00123456
0+004 <[^>]+> ef876543 	(swi|svc)	0x00876543
0+008 <[^>]+> ef123456 	(swi|svc)	0x00123456
0+00c <[^>]+> ef876543 	(swi|svc)	0x00876543
0+010 <[^>]+> df5a      	(swi|svc)	90
0+012 <[^>]+> dfa5      	(swi|svc)	165
0+014 <[^>]+> df5a      	(swi|svc)	90
0+016 <[^>]+> dfa5      	(swi|svc)	165
