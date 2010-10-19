#objdump: -dr --prefix-addresses --show-raw-insn
#name: Case Sensitive Register Aliases

.*: +file format .*arm.*

Disassembly of section .text:
0+0 <.*> ee060f10 	mcr	15, 0, r0, cr6, cr0, \{0\}
0+4 <.*> e1a00000 	nop			\(mov r0,r0\)
0+8 <.*> e1a00000 	nop			\(mov r0,r0\)
0+c <.*> e1a00000 	nop			\(mov r0,r0\)
