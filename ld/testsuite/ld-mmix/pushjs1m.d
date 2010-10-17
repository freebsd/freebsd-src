#source: nop123.s
#source: pushja.s
#source: pad2p18m32.s
#source: pad16.s
#source: pad4.s
#source: a.s
#source: start.s
#ld: -m mmo
#objdump: -dr

# Check that PUSHJ with an offset just within the offset range gets no
# stub expansion, mmo version.

.*:     file format mmo
Disassembly of section \.text:
0+ <pushja-0x4>:
       0:	fd010203 	swym 1,2,3
0+4 <pushja>:
       4:	e3fd0002 	setl \$253,0x2
       8:	f20cffff 	pushj \$12,40004 <a>
       c:	e3fd0003 	setl \$253,0x3
	\.\.\.
0+40004 <a>:
   40004:	e3fd0004 	setl \$253,0x4
0+40008 <Main>:
   40008:	e3fd0001 	setl \$253,0x1
