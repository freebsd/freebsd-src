#source: start.s
#source: jumpa.s
#source: pad2p26m32.s
#source: pad16.s
#source: pad4.s
#source: a.s
#as: -no-expand
#ld: -m mmo
#objdump: -dr

.*:     file format mmo

Disassembly of section \.text:

0+ <(Main|_start)>:
       0:	e3fd0001 	setl \$253,0x1

0+4 <jumpa>:
       4:	e3fd0002 	setl \$253,0x2
       8:	f0ffffff 	jmp 4000004 <a>
       c:	e3fd0003 	setl \$253,0x3
	\.\.\.

0+4000004 <a>:
 4000004:	e3fd0004 	setl \$253,0x4
