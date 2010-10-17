#as:
#objdump: -rs
#name: reloc-1

.*: +file format .*

RELOCATION RECORDS FOR \[\.text\]:
OFFSET   TYPE              VALUE 
0*000 R_XSTORMY16_16    global
0*002 R_XSTORMY16_16    global\+0x00000003
0*004 R_XSTORMY16_PC16  global\+0xfffffffc
0*006 R_XSTORMY16_32    global
0*00a R_XSTORMY16_32    global\+0x00000003
0*00e R_XSTORMY16_PC32  global\+0xfffffff2
0*012 R_XSTORMY16_8     global
0*013 R_XSTORMY16_8     global\+0xffff8100
0*014 R_XSTORMY16_8     global\+0x00000003
0*015 R_XSTORMY16_PC8   global\+0xffffffeb
0*016 R_XSTORMY16_16    dglobal
0*018 R_XSTORMY16_16    dwglobal


Contents of section \.text:
 0000 00000000 00000000 00000000 00000000  \................
 0010 00000000 00000000 0000               \..........      

