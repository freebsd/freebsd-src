#source: nop123.s
#source: nop123.s
#source: a.s
#source: pad2p18m32.s
#source: pad16.s
#source: pad4.s
#source: pad4.s
#source: pad4.s
#source: pushja.s
#source: start.s
#ld: -m mmo
#objdump: -dr

# Check that PUSHJ with an offset just outside the offset range gets a JMP
# stub expansion, backwards, mmo version.

.*:     file format mmo
Disassembly of section \.text:
0+ <a-0x8>:
       0:	fd010203 	swym 1,2,3
       4:	fd010203 	swym 1,2,3
0+8 <a>:
       8:	e3fd0004 	setl \$253,0x4
	\.\.\.
0+40008 <pushja>:
   40008:	e3fd0002 	setl \$253,0x2
   4000c:	f20c0002 	pushj \$12,40014 <pushja\+0xc>
   40010:	e3fd0003 	setl \$253,0x3
   40014:	f1fefffd 	jmp 8 <a>
0+40018 <Main>:
   40018:	e3fd0001 	setl \$253,0x1
