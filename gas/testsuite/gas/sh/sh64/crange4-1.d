#as: --abi=32
#objdump: -sr
#source: crange4.s
#name: .cranges descriptors with final variant.

.*:     file format .*-sh64.*

RELOCATION RECORDS FOR \[\.cranges\]:
OFFSET  *TYPE  *VALUE 
0+ R_SH_DIR32        \.text
0+a R_SH_DIR32        \.text


Contents of section \.text:
 0000 6ff0fff0 00000000 00000000 00000000  .*
 0010 00000000 00000000                    .*
Contents of section \.cranges:
 0000 00000000 00000004 00030000 00040000  .*
 0010 00140001                             .*
