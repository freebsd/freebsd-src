#source: ovl2.s
#ld: -N -T ovl.lnk --emit-relocs
#objdump: -D -r

.*elf32-spu

Disassembly of section \.text:

00000100 <_start>:
 100:	33 00 06 00 	brsl	\$0,130 <00000000\.ovl_call\.f1_a1>	# 130
			100: SPU_REL16	f1_a1
 104:	33 00 03 80 	brsl	\$0,120 <00000000\.ovl_call\.10:4>	# 120
			104: SPU_REL16	setjmp
 108:	32 7f ff 00 	br	100 <_start>	# 100
			108: SPU_REL16	_start

0000010c <setjmp>:
 10c:	35 00 00 00 	bi	\$0

00000110 <longjmp>:
 110:	35 00 00 00 	bi	\$0
	...

00000120 <00000000\.ovl_call.10:4>:
 120:	42 00 86 4f 	ila	\$79,268	# 10c
 124:	40 20 00 00 	nop	\$0
 128:	42 00 00 4e 	ila	\$78,0
 12c:	32 00 0a 80 	br	180 <__ovly_load>	# 180

00000130 <00000000\.ovl_call.f1_a1>:
 130:	42 02 00 4f 	ila	\$79,1024	# 400
 134:	40 20 00 00 	nop	\$0
 138:	42 00 00 ce 	ila	\$78,1
 13c:	32 00 08 80 	br	180 <__ovly_load>	# 180

00000140 <_SPUEAR_f1_a2>:
 140:	42 02 00 4f 	ila	\$79,1024	# 400
 144:	40 20 00 00 	nop	\$0
 148:	42 00 01 4e 	ila	\$78,2
 14c:	32 00 06 80 	br	180 <__ovly_load>	# 180
#...
Disassembly of section \.ov_a1:

00000400 <f1_a1>:
 400:	35 00 00 00 	bi	\$0
	\.\.\.
Disassembly of section \.ov_a2:

00000400 <f1_a2>:
 400:	32 7f a2 00 	br	110 <longjmp>	# 110
			400: SPU_REL16	longjmp
	\.\.\.
Disassembly of section \.data:

00000410 <_ovly_table>:
 410:	00 00 04 00 	.*
 414:	00 00 00 10 	.*
 418:	00 00 02 d0 	.*
 41c:	00 00 00 01 	.*
 420:	00 00 04 00 	.*
 424:	00 00 00 10 	.*
 428:	00 00 02 e0 	.*
 42c:	00 00 00 01 	.*

00000430 <_ovly_buf_table>:
 430:	00 00 00 00 	.*
Disassembly of section \.toe:

00000440 <_EAR_>:
	\.\.\.
Disassembly of section \.note\.spu_name:

.* <\.note\.spu_name>:
.*:	00 00 00 08 .*
.*:	00 00 00 0c .*
.*:	00 00 00 01 .*
.*:	53 50 55 4e .*
.*:	41 4d 45 00 .*
.*:	74 6d 70 64 .*
.*:	69 72 2f 64 .*
.*:	75 6d 70 00 .*
