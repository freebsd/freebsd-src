#objdump: -dr
#as: -linkrelax -no-expand -x
#source: relax1.s
#
# This test-case assumes that out-of-range errors (still) cause
# relocs to be emitted, rather than errors emitted.  FIXME.

.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
       0:	f0000000 	jmp 0 <Main>
			0: R_MMIX_ADDR27	\.text\+0x80024

0000000000000004 <l0>:
       4:	f0000000 	jmp 4 <l0>
			4: R_MMIX_ADDR27	\.text\+0x80024

0000000000000008 <l1>:
       8:	f0000000 	jmp 8 <l1>
			8: R_MMIX_ADDR27	\.text\+0x80024

000000000000000c <l01>:
       c:	f0000000 	jmp c <l01>
			c: R_MMIX_ADDR27	\.text\+0x80024
      10:	f4070000 	geta \$7,10 <l01\+0x4>
			10: R_MMIX_ADDR19	\.text\+0x4000c
      14:	f2bf0000 	pushj \$191,14 <l01\+0x8>
			14: R_MMIX_ADDR19	\.text\+0x40010

0000000000000018 <l2>:
      18:	f0000000 	jmp 18 <l2>
			18: R_MMIX_ADDR27	\.text\+0x40010
	\.\.\.
   40004:	4c480000 	bnp \$72,40004 <l2\+0x3ffec>
			40004: R_MMIX_ADDR19	\.text\+0x4
   40008:	f4040000 	geta \$4,40008 <l2\+0x3fff0>
			40008: R_MMIX_ADDR19	\.text\+0x8

000000000004000c <nearfar1>:
   4000c:	f2050000 	pushj \$5,4000c <nearfar1>
			4000c: R_MMIX_ADDR19	\.text\+0xc

0000000000040010 <nearfar2>:
   40010:	f4090000 	geta \$9,40010 <nearfar2>
			40010: R_MMIX_ADDR19	\.text\+0x8
   40014:	f20b0000 	pushj \$11,40014 <nearfar2\+0x4>
			40014: R_MMIX_ADDR19	\.text\+0x80014

0000000000040018 <l4>:
   40018:	44370000 	bp \$55,40018 <l4>
			40018: R_MMIX_ADDR19	\.text\+0x80014
	\.\.\.
   80010:	f0000000 	jmp 80010 <l4\+0x3fff8>
			80010: R_MMIX_ADDR27	\.text\+0x8

0000000000080014 <l3>:
   80014:	f0000000 	jmp 80014 <l3>
			80014: R_MMIX_ADDR27	\.text\+0x4
   80018:	46580000 	bod \$88,80018 <l3\+0x4>
			80018: R_MMIX_ADDR19	\.text\+0x40018
   8001c:	46580000 	bod \$88,8001c <l3\+0x8>
			8001c: R_MMIX_ADDR19	\.text\+0x40018
   80020:	f0000000 	jmp 80020 <l3\+0xc>
			80020: R_MMIX_ADDR27	\.text\+0x4080020

0000000000080024 <l6>:
   80024:	f0000000 	jmp 80024 <l6>
			80024: R_MMIX_ADDR27	\.text\+0x4080020
   80028:	426f0000 	bz \$111,80028 <l6\+0x4>
			80028: R_MMIX_ADDR19	\.text\+0x80014
	\.\.\.

0000000004080020 <l5>:
 4080020:	f0000000 	jmp 4080020 <l5>
			4080020: R_MMIX_ADDR27	\.text\+0x4080030
 4080024:	f0000000 	jmp 4080024 <l5\+0x4>
			4080024: R_MMIX_ADDR27	\.text\+0x80024
 4080028:	f0000000 	jmp 4080028 <l5\+0x8>
			4080028: R_MMIX_ADDR27	\.text\+0x80024
 408002c:	482c0000 	bnn \$44,408002c <l5\+0xc>
			408002c: R_MMIX_ADDR19	\.text\+0x40c002c

0000000004080030 <l8>:
 4080030:	482c0000 	bnn \$44,4080030 <l8>
			4080030: R_MMIX_ADDR19	\.text\+0x40c002c
 4080034:	f0000000 	jmp 4080034 <l8\+0x4>
			4080034: R_MMIX_ADDR27	\.text\+0x4080020
 4080038:	f0000000 	jmp 4080038 <l8\+0x8>
			4080038: R_MMIX_ADDR27	\.text\+0x4080020
	\.\.\.

00000000040c0028 <l10>:
 40c0028:	f0000000 	jmp 40c0028 <l10>
			40c0028: R_MMIX_ADDR27	\.text\+0x4080020

00000000040c002c <l9>:
 40c002c:	f0000000 	jmp 40c002c <l9>
			40c002c: R_MMIX_ADDR27	\.text\+0x40c0038

00000000040c0030 <l7>:
 40c0030:	f2210000 	pushj \$33,40c0030 <l7>
			40c0030: R_MMIX_ADDR19	\.text\+0x4080030
 40c0034:	f2210000 	pushj \$33,40c0034 <l7\+0x4>
			40c0034: R_MMIX_ADDR19	\.text\+0x4080030

00000000040c0038 <l11>:
 40c0038:	f0000000 	jmp 40c0038 <l11>
			40c0038: R_MMIX_ADDR27	\.text\+0x4080020
 40c003c:	f0000000 	jmp 40c003c <l11\+0x4>
			40c003c: R_MMIX_ADDR27	\.text\+0x4080030
	\.\.\.
 4100038:	f43d0000 	geta \$61,4100038 <l11\+0x40000>
			4100038: R_MMIX_ADDR19	\.text\+0x40c0038
 410003c:	f4480000 	geta \$72,410003c <l11\+0x40004>
			410003c: R_MMIX_ADDR19	\.text\+0x40c0038
