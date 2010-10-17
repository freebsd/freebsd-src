#source: start.s
#source: a.s
#source: getaa.s
#as: -x
#ld: -m elf64mmix
#objdump: -dr

.*:     file format elf64-mmix

Disassembly of section \.text:

0+ <_start>:
   0+:	e3fd0001 	setl \$253,0x1

0+4 <a>:
   4:	e3fd0004 	setl \$253,0x4

0+8 <getaa>:
   8:	e3fd0002 	setl \$253,0x2
   c:	e37b0004 	setl \$123,0x4
  10:	e67b0000 	incml \$123,0x0
  14:	e57b0000 	incmh \$123,0x0
  18:	e47b0000 	inch \$123,0x0
  1c:	e3fd0003 	setl \$253,0x3
