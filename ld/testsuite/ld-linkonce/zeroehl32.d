#source: x.s
#source: y.s
#ld: -Ttext 0xa00 -T zeroeh.ld
#objdump: -s
#target: cris-*-elf cris-*-linux* i?86-*-elf i?86-*-linux*

# The word at address 201c, for the linkonce-excluded section, must be zero.

.*:     file format elf32.*

Contents of section \.text:
 0a00 080a0000 100a0000 01000000 02000000  .*
 0a10 03000000                             .*
Contents of section \.gcc_except_table:
 2000 02000000 080a0000 08000000 07000000  .*
 2010 100a0000 04000000 66600000 00000000  .*
 2020 04000000                             .*
Contents of section \.eh_frame:
 4000 0c000000 00000000 0100017c 00000000  .*
 4010 0c000000 14000000 080a0000 08000000  .*
 4020 0c000000 24000000 100a0000 04000000  .*
