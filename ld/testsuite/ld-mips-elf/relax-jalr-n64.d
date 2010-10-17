#name: MIPS relax-jalr n64
#source: relax-jalr.s
#as: -KPIC -64 -EB
#objdump: --prefix-addresses -d --show-raw-insn
#ld: --relax -melf64btsmip

.*:     file format elf.*mips.*

Disassembly of section \.text:
	\.\.\.
	\.\.\.
.*	ld	t9,.*
.*	bal	.* <__start>
.*	nop
	\.\.\.
.*	ld	t9,.*
.*	bal	.* <__start>
.*	nop
	\.\.\.
.*	ld	t9,.*
.*	bal	.* <__start>
.*	nop
	\.\.\.
