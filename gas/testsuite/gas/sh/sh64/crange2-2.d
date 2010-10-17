#as: --abi=32
#objdump: -sr
#source: crange2.s
#name: .cranges descriptors for SHcompact and SHmedia in .text.

.*:     file format .*-sh64.*

RELOCATION RECORDS FOR \[\.cranges\]:

OFFSET  *TYPE  *VALUE 
0+0 R_SH_DIR32        \.text
0+a R_SH_DIR32        \.text
0+14 R_SH_DIR32        \.text


Contents of section \.text:
 0000 e8000a30 ec001240 ec001250 6ff0fff0  .*
 0010 00090009 00090009 00090009 00090009  .*
 0020 00090009 effffa60 effffa70 ebffe200  .*
Contents of section .cranges:
 0000 00000000 00000010 00030000 00100000  .*
 0010 00140002 00000024 0000000c 0003      .*
