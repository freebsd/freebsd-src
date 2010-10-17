#source: start.s
#source: a.s
#source: pad2p26m32.s
#source: pad16.s
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

0+4000000 <jumpa>:
 4000000:	e3fd0002 	setl \$253,0x2
 4000004:	f1000000 	jmp 4 <a>
 4000008:	fd000000 	swym 0,0,0
 400000c:	fd000000 	swym 0,0,0
 4000010:	fd000000 	swym 0,0,0
 4000014:	fd000000 	swym 0,0,0
 4000018:	e3fd0003 	setl \$253,0x3
