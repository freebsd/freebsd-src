#source: start.s
#source: a.s
#source: pad2p26m32.s
#source: pad16.s
#source: pad4.s
#source: pad4.s
#source: pad4.s
#source: jumpa.s
#as: -x
#ld: -m mmo
#objdump: -dr

.*:     file format mmo

Disassembly of section \.text:

0+ <(Main|_start)>:
       0:	e3fd0001 	setl \$253,0x1

0+4 <a>:
       4:	e3fd0004 	setl \$253,0x4
	\.\.\.

0+4000004 <jumpa>:
 4000004:	e3fd0002 	setl \$253,0x2
 4000008:	e3ff0004 	setl \$255,0x4
 400000c:	e6ff0000 	incml \$255,0x0
 4000010:	e5ff0000 	incmh \$255,0x0
 4000014:	e4ff0000 	inch \$255,0x0
 4000018:	9fffff00 	go \$255,\$255,0
 400001c:	e3fd0003 	setl \$253,0x3
