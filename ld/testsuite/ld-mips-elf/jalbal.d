#name: jal to bal
#source: jalbal.s
#as: -EB -n32 -march=rm9000
#ld: -EB -e s1 -Ttext 0x100000a0
#objdump: -d

.*file format elf.*mips.*

Disassembly of section \.text:

.* <s1>:
.*	0c00802a 	jal	.*100200a8 <s3>
.*	00000000 	nop
.*	04117fff 	bal	.*100200a8 <s3>

.* <s2>:
.*	\.\.\.

.* <s3>:
.*	04118000 	bal	.*100000ac <s2>
.*	00000000 	nop
.*	0c00002b 	jal	.*100000ac <s2>
.*	00000000 	nop
.*	\.\.\.
