#objdump: -D
#source: error-001.s

# Test expect's dump_run_test baseline

.*: +file format elf32-d10v

Disassembly of section .text:

00000000 <foo>:
   0:	20 01 5e 00 	mv	r0, r1	||	nop	
