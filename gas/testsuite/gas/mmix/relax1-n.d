#objdump: -dr
#as: -no-expand -x
#source: relax1.s
#
# This test-case assumes that out-of-range errors cause relocs to
# be emitted, rather than errors emitted.  FIXME.

.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
       0:	f0020009 	jmp 80024 <l6>

0000000000000004 <l0>:
       4:	f0020008 	jmp 80024 <l6>

0000000000000008 <l1>:
       8:	f0020007 	jmp 80024 <l6>

000000000000000c <l01>:
       c:	f0020006 	jmp 80024 <l6>
      10:	f407ffff 	geta \$7,4000c <nearfar1>
      14:	f2bfffff 	pushj \$191,40010 <nearfar2>

0000000000000018 <l2>:
      18:	f000fffe 	jmp 40010 <nearfar2>
	\.\.\.
   40004:	4d480000 	bnp \$72,4 <l0>
   40008:	f5040000 	geta \$4,8 <l1>

000000000004000c <nearfar1>:
   4000c:	f3050000 	pushj \$5,c <l01>

0000000000040010 <nearfar2>:
   40010:	f4090000 	geta \$9,40010 <nearfar2>
			40010: R_MMIX_ADDR19	\.text\+0x8
   40014:	f20b0000 	pushj \$11,40014 <nearfar2\+0x4>
			40014: R_MMIX_ADDR19	\.text\+0x80014

0000000000040018 <l4>:
   40018:	4437ffff 	bp \$55,80014 <l3>
	...
   80010:	f1fdfffe 	jmp 8 <l1>

0000000000080014 <l3>:
   80014:	f1fdfffc 	jmp 4 <l0>
   80018:	47580000 	bod \$88,40018 <l4>
   8001c:	46580000 	bod \$88,8001c <l3\+0x8>
			8001c: R_MMIX_ADDR19	\.text\+0x40018
   80020:	f0000000 	jmp 80020 <l3\+0xc>
			80020: R_MMIX_ADDR27	\.text\+0x4080020

0000000000080024 <l6>:
   80024:	f0ffffff 	jmp 4080020 <l5>
   80028:	436ffffb 	bz \$111,80014 <l3>
	\.\.\.

0000000004080020 <l5>:
 4080020:	f0000004 	jmp 4080030 <l8>
 4080024:	f1000000 	jmp 80024 <l6>
 4080028:	f0000000 	jmp 4080028 <l5\+0x8>
			4080028: R_MMIX_ADDR27	\.text\+0x80024
 408002c:	482c0000 	bnn \$44,408002c <l5\+0xc>
			408002c: R_MMIX_ADDR19	\.text\+0x40c002c

0000000004080030 <l8>:
 4080030:	482cffff 	bnn \$44,40c002c <l9>
 4080034:	f1fffffb 	jmp 4080020 <l5>
 4080038:	f1fffffa 	jmp 4080020 <l5>
	\.\.\.

00000000040c0028 <l10>:
 40c0028:	f1fefffe 	jmp 4080020 <l5>

00000000040c002c <l9>:
 40c002c:	f0000003 	jmp 40c0038 <l11>

00000000040c0030 <l7>:
 40c0030:	f3210000 	pushj \$33,4080030 <l8>
 40c0034:	f2210000 	pushj \$33,40c0034 <l7\+0x4>
			40c0034: R_MMIX_ADDR19	\.text\+0x4080030

00000000040c0038 <l11>:
 40c0038:	f1fefffa 	jmp 4080020 <l5>
 40c003c:	f1fefffd 	jmp 4080030 <l8>
	\.\.\.
 4100038:	f53d0000 	geta \$61,40c0038 <l11>
 410003c:	f4480000 	geta \$72,410003c <l11\+0x40004>
			410003c: R_MMIX_ADDR19	\.text\+0x40c0038
