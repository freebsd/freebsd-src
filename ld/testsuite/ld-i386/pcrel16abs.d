#name: PCREL16 absolute reloc
#as: --32
#ld: -melf_i386 -Ttext 0xfffffff0
#objdump: -drj.text -m i8086

.*: +file format elf32-i386

Disassembly of section .text:

f+0 <_start>:
f+0:	e9 0d e0[ 	]+jmp[ 	]+ffffe000 <SEGMENT_SIZE\+0xfffee000>
#pass
