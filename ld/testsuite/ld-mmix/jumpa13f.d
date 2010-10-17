#source: start.s
#source: jumpa.s
#source: pad2p26m32.s
#source: pad4.s
#source: pad4.s
#source: a.s
#as: -x
#ld: -m mmo
#objdump: -dr

.*:     file format mmo

Disassembly of section \.text:

0+ <(Main|_start)>:
       0:	e3fd0001 	setl \$253,0x1

0+4 <jumpa>:
       4:	e3fd0002 	setl \$253,0x2
       8:	e3ff0008 	setl \$255,0x8
       c:	e6ff0400 	incml \$255,0x400
      10:	e5ff0000 	incmh \$255,0x0
      14:	e4ff0000 	inch \$255,0x0
      18:	9fffff00 	go \$255,\$255,0
      1c:	e3fd0003 	setl \$253,0x3
	\.\.\.

0+4000008 <a>:
 4000008:	e3fd0004 	setl \$253,0x4
