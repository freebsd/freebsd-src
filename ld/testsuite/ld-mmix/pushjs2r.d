#source: nop123.s
#source: pushja.s
#source: ext1l.s
#source: pad2p18m32.s
#source: pad16.s
#source: nop123.s
#ld: -r -m elf64mmix
#objdump: -dr

# When linking relocatably, check that PUSHJ with a distance to the end of
# the section just outside the offset range gets expanded.

.*:     file format elf64-mmix
Disassembly of section \.text:
0+ <pushja-0x4>:
       0:	fd010203 	swym 1,2,3
0+4 <pushja>:
       4:	e3fd0002 	setl \$253,0x2
       8:	f20c0002 	pushj \$12,10 <pushja\+0xc>
       c:	e3fd0003 	setl \$253,0x3
      10:	f0000000 	jmp 10 <pushja\+0xc>
			10: R_MMIX_JMP	a
	\.\.\.
0+24 <ext1>:
      24:	fd040810 	swym 4,8,16
	\.\.\.
   40018:	fd010203 	swym 1,2,3
