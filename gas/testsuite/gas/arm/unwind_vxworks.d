#objdump: -sr
#name: Unwind table generation
# This is the VxWorks variant of this file.
#source: unwind.s
#not-skip: *-*-vxworks*

.*:     file format.*

RELOCATION RECORDS FOR \[.ARM.extab\]:
OFFSET   TYPE              VALUE 
0000000c R_ARM_PREL31      .text


RELOCATION RECORDS FOR \[.ARM.exidx\]:
OFFSET   TYPE              VALUE 
00000000 R_ARM_PREL31      .text
00000000 R_ARM_NONE        __aeabi_unwind_cpp_pr0
00000008 R_ARM_PREL31      .text.*\+0x00000004
00000008 R_ARM_NONE        __aeabi_unwind_cpp_pr1
0000000c R_ARM_PREL31      .ARM.extab
00000010 R_ARM_PREL31      .text.*\+0x00000008
00000014 R_ARM_PREL31      .ARM.extab.*\+0x0000000c
00000018 R_ARM_PREL31      .text.*\+0x0000000c
0000001c R_ARM_PREL31      .ARM.extab.*\+0x0000001c
00000020 R_ARM_PREL31      .text.*\+0x00000010
00000028 R_ARM_PREL31      .text.*\+0x00000012


Contents of section .text:
 0000 (0000a0e3 0100a0e3 0200a0e3 0300a0e3|e3a00000 e3a00001 e3a00002 e3a00003)  .*
 0010 (04200520|20052004)                             .*
Contents of section .ARM.extab:
 0000 (449b0181 b0b08086|81019b44 8680b0b0) 00000000 00000000  .*
 0010 (8402b101 b0b0b005 2a000000 00c60181|01b10284 05b0b0b0 0000002a 8101c600)  .*
 0020 (b0b0c1c1|c1c1b0b0) 00000000                    .*
Contents of section .ARM.exidx:
 0000 00000000 (b0b0a880|80a8b0b0) 00000000 00000000  .*
 0010 00000000 00000000 00000000 00000000  .*
 0020 00000000 (08849780|80978408) 00000000 (b00fb180|80b10fb0)  .*
# Ignore .ARM.attributes section
#...
