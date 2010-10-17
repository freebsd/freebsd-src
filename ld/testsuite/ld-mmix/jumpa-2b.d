#source: start.s
#source: a.s
#source: jumpa.s
#as: -no-expand
#ld: -m elf64mmix
#objdump: -dr

.*:     file format elf64-mmix

Disassembly of section \.text:

0+ <_start>:
   0:	e3fd0001 	setl \$253,0x1

0+4 <a>:
   4:	e3fd0004 	setl \$253,0x4

0+8 <jumpa>:
   8:	e3fd0002 	setl \$253,0x2
   c:	f1fffffe 	jmp 4 <a>
  10:	e3fd0003 	setl \$253,0x3
