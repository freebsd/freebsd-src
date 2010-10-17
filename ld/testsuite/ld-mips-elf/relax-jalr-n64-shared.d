#name: MIPS relax-jalr-shared n64
#source: relax-jalr.s
#as: -KPIC -64 -EB
#objdump: --prefix-addresses -d --show-raw-insn
#ld: --relax -shared -melf64btsmip

.*:     file format elf.*mips.*

Disassembly of section \.text:
	\.\.\.
	\.\.\.
.*	ld	t9,.*
.*	jalr	t9
.*	nop
	\.\.\.
.*	ld	t9,.*
.*	jalr	t9
.*	nop
	\.\.\.
.*	ld	t9,.*
.*	bal	.* <__start>
.*	nop
	\.\.\.
Disassembly of section \.MIPS\.stubs:
	\.\.\.
