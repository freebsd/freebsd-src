#source: nop123.s
#source: pushja.s
#source: pad2p26m32.s
#source: pad16.s
#source: pad4.s
#source: pad4.s
#source: pad4.s
#source: a.s
#source: start.s
#ld: -m mmo
#objdump: -dr

# Check that PUSHJ with an offset just outside reach of JMP works; it will
# get the full expansion, mmo version.

.*:     file format mmo
Disassembly of section \.text:
0+ <pushja-0x4>:
       0:	fd010203 	swym 1,2,3
0+4 <pushja>:
       4:	e3fd0002 	setl \$253,0x2
       8:	f20c0002 	pushj \$12,10 <pushja\+0xc>
       c:	e3fd0003 	setl \$253,0x3
      10:	e3ff0020 	setl \$255,0x20
      14:	e6ff0400 	incml \$255,0x400
      18:	e5ff0000 	incmh \$255,0x0
      1c:	e4ff0000 	inch \$255,0x0
      20:	9f00ff00 	go \$0,\$255,0
	\.\.\.
0+4000020 <a>:
 4000020:	e3fd0004 	setl \$253,0x4
0+4000024 <Main>:
 4000024:	e3fd0001 	setl \$253,0x1
