#as: --abi=32
#objdump: -sr
#source: crange3.s
#name: .cranges descriptors, constant mix.

.*:     file format .*-sh64.*

RELOCATION RECORDS FOR \[\.cranges\]:
OFFSET  *TYPE  *VALUE 
0+00 R_SH_DIR32        \.text
0+0a R_SH_DIR32        \.text
0+14 R_SH_DIR32        \.text


Contents of section \.text:
 0000 6ff0fff0 6ff0fff0 6ff0fff0 6ff0fff0  .*
 0010 01235678 12345678 12345678 1234fede  .*
 0020 6ff0fff0 6ff0fff0 6ff0fff0 6ff0fff0  .*
 0030 6ff0fff0                             .*
Contents of section \.rodata:
 0000 abcdef01 12345678                    .*
Contents of section \.cranges:
 0000 00000000 00000010 00030000 00100000  .*
 0010 00100001 00000020 00000014 0003      .*
