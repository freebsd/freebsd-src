#name: arch-cpu-1
#objdump: -dp


.*:     file format elf32-m68k
private flags = 21: \[isa A\] \[nodiv\] \[emac\]

Disassembly of section .text:

00000000 <.text>:
   0:	a241 0280      	macw %d1l,%a1u,<<,%acc0
