#as: -x
#source: weak1.s
#objdump: -str

# Like weak1, but with PUSHJ stubs.

.*:     file format elf64-mmix
SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+  w      \.text	0+ foo
0+4 g       \.text	0+ main
RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+8 R_MMIX_64         foo
0+4 R_MMIX_PUSHJ_STUBBABLE  foo
Contents of section \.text:
 0000 f8010000 f20f0000 00000000 00000000  .*
