#source: greg-1.s
#source: gregldo1.s
#source: start.s
#ld: -m elf64mmix
#objdump: -dt

# Most simple greg usage: relocate to each possible location within an
# insn.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+7f0 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+c g       \.text	0+ _start
0+fe g       \*REG\*	0+ areg
#...

Disassembly of section \.text:

0+ <_start-0xc>:
   0:	8c0c20fe 	ldo \$12,\$32,\$254
   4:	8d7bfe22 	ldo \$123,\$254,34
   8:	8dfeea38 	ldo \$254,\$234,56

0+c <_start>:
   c:	e3fd0001 	setl \$253,0x1
