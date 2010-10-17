#source: nop123.s
#source: pushja.s
#source: pad2p18m32.s
#source: pad16.s
#source: nop123.s
#ld: -r -m elf64mmix
#objdump: -dr

# When linking relocatable, check that PUSHJ with a distance to the end of
# the section just within the offset range gets no stub expansion.

.*:     file format elf64-mmix
Disassembly of section \.text:
0+ <pushja-0x4>:
       0:	fd010203 	swym 1,2,3
0+4 <pushja>:
       4:	e3fd0002 	setl \$253,0x2
       8:	f20c0000 	pushj \$12,8 <pushja\+0x4>
			8: R_MMIX_PUSHJ_STUBBABLE	a
       c:	e3fd0003 	setl \$253,0x3
	\.\.\.
   40000:	fd010203 	swym 1,2,3
