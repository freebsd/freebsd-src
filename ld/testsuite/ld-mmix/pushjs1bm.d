#source: nop123.s
#source: nop123.s
#source: a.s
#source: pad2p18m32.s
#source: pad16.s
#source: pad4.s
#source: pad4.s
#source: pushja.s
#source: start.s
#ld: -m mmo
#objdump: -dr

# Check that PUSHJ with an offset just within the offset range gets no
# stub expansion, backwards, mmo version.

.*:     file format mmo
Disassembly of section \.text:
0+ <a-0x8>:
       0:	fd010203 	swym 1,2,3
       4:	fd010203 	swym 1,2,3
0+8 <a>:
       8:	e3fd0004 	setl \$253,0x4
	\.\.\.
0+40004 <pushja>:
   40004:	e3fd0002 	setl \$253,0x2
   40008:	f30c0000 	pushj \$12,8 <a>
   4000c:	e3fd0003 	setl \$253,0x3
0+40010 <Main>:
   40010:	e3fd0001 	setl \$253,0x1
