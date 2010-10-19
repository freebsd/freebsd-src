#objdump: -r
#name: reloc
.*: +file format .*

RELOCATION RECORDS FOR \[\.text\]:
OFFSET   TYPE              VALUE 
0*0004 R_pcrel24         _call_data1
0*0008 R_rimm16          .data
0*000a R_pcrel12_jump_s  .text\+0x00000018
0*000e R_pcrel24         call_data1\+0x00000008
0*0012 R_huimm16         .data\+0x00000002
0*0016 R_luimm16         .data\+0x00000004
0*001a R_huimm16         load_extern1


RELOCATION RECORDS FOR \[\.data\]:
OFFSET   TYPE              VALUE 
0*0006 R_byte_data       load_extern1


