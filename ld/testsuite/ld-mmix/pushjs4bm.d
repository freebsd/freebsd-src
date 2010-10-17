#source: nop123.s
#source: nop123.s
#source: a.s
#source: pad2p26m32.s
#source: pad16.s
#source: pad4.s
#source: pushja.s
#source: start.s
#ld: -m mmo
#objdump: -dr

# Check that PUSHJ with an offset just outside the offset range of a JMP
# stub expansion works, backwards, mmo version.

.*:     file format mmo
Disassembly of section \.text:
0+ <a-0x8>:
       0:	fd010203 	swym 1,2,3
       4:	fd010203 	swym 1,2,3
0+8 <a>:
       8:	e3fd0004 	setl \$253,0x4
	\.\.\.
0+4000000 <pushja>:
 4000000:	e3fd0002 	setl \$253,0x2
 4000004:	f20c0002 	pushj \$12,400000c <pushja\+0xc>
 4000008:	e3fd0003 	setl \$253,0x3
 400000c:	e3ff0008 	setl \$255,0x8
 4000010:	e6ff0000 	incml \$255,0x0
 4000014:	e5ff0000 	incmh \$255,0x0
 4000018:	e4ff0000 	inch \$255,0x0
 400001c:	9f00ff00 	go \$0,\$255,0
0+4000020 <Main>:
 4000020:	e3fd0001 	setl \$253,0x1
