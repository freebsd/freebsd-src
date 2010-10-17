#as: --isa=shmedia --abi=64
#objdump: -d
#name: Minimum SH64 Syntax Support - Pseudos.

dump.o:     file format elf64-sh64.*

Disassembly of section .text:

0000000000000000 <.*>:
   0:	e8000a00 	pta/l	8 <.*>,tr0
   4:	ec000600 	ptb/l	8 <.*>,tr0
