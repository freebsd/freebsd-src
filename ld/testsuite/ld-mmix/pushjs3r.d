#source: nop123.s
#source: pushja.s
#source: undef-2.s
#source: nop123.s
#source: pad16.s
#source: pad2p18m32.s
#ld: -r -m elf64mmix
#objdump: -dr

# When linking relocatably, check two expanded stubbable PUSHJs.

# With better relaxation support for relocatable links, both should be
# able to pass through unexpanded.  Right now, we just check that they can
# coexist peacefully.

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
      24:	f2050001 	pushj \$5,28 <pushja\+0x24>
      28:	f0000000 	jmp 28 <pushja\+0x24>
			28: R_MMIX_JMP	undefd
	\.\.\.
      3c:	fd010203 	swym 1,2,3
	\.\.\.
