#source: start.s
#source: bpo-8.s
#source: areg-t.s
#as: -linker-allocated-gregs
#ld: -m elf64mmix
#objdump: -st

# A BPO and another reloc in the same section.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+7f0 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ g       \.text	0+ _start
0+10 g       \.text	0+ areg
#...

Contents of section \.text:
 0000 e3fd0001 2336fe00 00000000 0000000c  .*
 0010 fd040810                             .*
Contents of section \.MMIX\.reg_contents:
 07f0 00000000 00000008                    .*
