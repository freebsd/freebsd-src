#source: nop123.s
#source: nop123.s
#source: a.s
#source: pad2p26m32.s
#source: pad16.s
#source: pushja.s
#source: start.s
#ld: -m mmo
#objdump: -dr

# Check that PUSHJ with an offset just inside the offset range of a JMP
# stub expansion works, backwards, mmo version.

.*:     file format mmo
Disassembly of section \.text:
0+ <a-0x8>:
       0:	fd010203 	swym 1,2,3
       4:	fd010203 	swym 1,2,3
0+8 <a>:
       8:	e3fd0004 	setl \$253,0x4
	\.\.\.
0+3fffffc <pushja>:
 3fffffc:	e3fd0002 	setl \$253,0x2
 4000000:	f20c0002 	pushj \$12,4000008 <pushja\+0xc>
 4000004:	e3fd0003 	setl \$253,0x3
 4000008:	f1000000 	jmp 8 <a>
0+400000c <Main>:
 400000c:	e3fd0001 	setl \$253,0x1
