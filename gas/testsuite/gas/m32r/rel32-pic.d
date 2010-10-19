#as: -KPIC
#objdump: -r
#name: rel32-pic

.*: +file format .*

RELOCATION RECORDS FOR \[.text2\]:
OFFSET   TYPE              VALUE 
00000000 R_M32R_REL32      .text\+0x00000004
00000008 R_M32R_REL32      .text\+0x00000008
0000000c R_M32R_REL32      .text


