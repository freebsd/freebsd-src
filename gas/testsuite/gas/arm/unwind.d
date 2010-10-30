#objdump: -sr
#name: Unwind table generation
# This test is only valid on ELF based ports.
#not-target: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*
# VxWorks needs a special variant of this file.
#skip: *-*-vxworks*

.*:     file format.*

RELOCATION RECORDS FOR \[.ARM.extab\]:
OFFSET   TYPE              VALUE 
0000000c R_ARM_PREL31      .text


RELOCATION RECORDS FOR \[.ARM.exidx\]:
OFFSET   TYPE              VALUE 
00000000 R_ARM_PREL31      .text
00000000 R_ARM_NONE        __aeabi_unwind_cpp_pr0
00000008 R_ARM_PREL31      .text.*
00000008 R_ARM_NONE        __aeabi_unwind_cpp_pr1
0000000c R_ARM_PREL31      .ARM.extab
00000010 R_ARM_PREL31      .text.*
00000014 R_ARM_PREL31      .ARM.extab.*
00000018 R_ARM_PREL31      .text.*
0000001c R_ARM_PREL31      .ARM.extab.*
00000020 R_ARM_PREL31      .text.*
00000028 R_ARM_PREL31      .text.*
00000030 R_ARM_PREL31      .text.*
00000034 R_ARM_PREL31      .ARM.extab.*


Contents of section .text:
 0000 (0000a0e3 0100a0e3 0200a0e3 0300a0e3|e3a00000 e3a00001 e3a00002 e3a00003)  .*
 0010 (04200520 0600a0e3|20052004 e3a00006)                    .*
Contents of section .ARM.extab:
 0000 (449b0181 b0b08086|81019b44 8680b0b0) 00000000 00000000  .*
 0010 (8402b101 b0b0b005 2a000000 00c60281|01b10284 05b0b0b0 0000002a 8102c600)  .*
 0020 (d0c6c1c1 b0b0c0c6|c1c1c6d0 c6c0b0b0) 00000000 (429b0181|81019b42)  .*
 0030 (b0008086|868000b0) 00000000                    .*
Contents of section .ARM.exidx:
 0000 00000000 (b0b0a880 04000000|80a8b0b0 00000004) 00000000  .*
 0010 (08000000 0c000000 0c000000 1c000000|00000008 0000000c 0000000c 0000001c)  .*
 0020 (10000000 08849780 12000000 b00fb180|00000010 80978408 00000012 80b10fb0)  .*
 0030 (14000000 2c000000|00000014 0000002c)                    .*
# Ignore .ARM.attributes section
#...
