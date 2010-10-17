#source: start.s
#source: bza.s
#source: a.s
#as: -x
#ld: -m elf64mmix
#objdump: -dr

.*:     file format elf64-mmix

Disassembly of section \.text:

0+ <_start>:
   0+:	e3fd0001 	setl \$253,0x1

0+4 <bza>:
   4:	e3fd0002 	setl \$253,0x2
   8:	5aea0006 	pbnz \$234,20 <bza\+0x1c>
   c:	e3ff0024 	setl \$255,0x24
  10:	e6ff0000 	incml \$255,0x0
  14:	e5ff0000 	incmh \$255,0x0
  18:	e4ff0000 	inch \$255,0x0
  1c:	9fffff00 	go \$255,\$255,0
  20:	e3fd0003 	setl \$253,0x3

0+24 <a>:
  24:	e3fd0004 	setl \$253,0x4
