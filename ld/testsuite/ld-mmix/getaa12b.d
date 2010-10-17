#source: start.s
#source: a.s
#source: pad2p18m32.s
#source: pad16.s
#source: pad4.s
#source: pad4.s
#source: getaa.s
#as: -no-expand
#ld: -m mmo
#objdump: -dr

.*:     file format mmo

Disassembly of section \.text:

0+ <(Main|_start)>:
       0:	e3fd0001 	setl \$253,0x1

0+4 <a>:
       4:	e3fd0004 	setl \$253,0x4
	\.\.\.

0+40000 <getaa>:
   40000:	e3fd0002 	setl \$253,0x2
   40004:	f57b0000 	geta \$123,4 <a>
   40008:	e3fd0003 	setl \$253,0x3
