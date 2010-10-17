#source: start.s
#source: jumpa.s
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
   8:	f0000006 	jmp 20 <a>
   c:	fd000000 	swym 0,0,0
  10:	fd000000 	swym 0,0,0
  14:	fd000000 	swym 0,0,0
  18:	fd000000 	swym 0,0,0
  1c:	e3fd0003 	setl \$253,0x3

0+20 <a>:
  20:	e3fd0004 	setl \$253,0x4
