#source: start.s
#source: getaa.s
#source: pad2p18m32.s
#source: pad16.s
#source: pad4.s
#source: a.s
#as: -no-expand
#ld: -m elf64mmix
#objdump: -dr

.*:     file format elf64-mmix

Disassembly of section \.text:

0+ <_start>:
       0:	e3fd0001 	setl \$253,0x1

0+4 <getaa>:
       4:	e3fd0002 	setl \$253,0x2
       8:	f47bffff 	geta \$123,40004 <a>
       c:	e3fd0003 	setl \$253,0x3
	\.\.\.

0+40004 <a>:
   40004:	e3fd0004 	setl \$253,0x4
