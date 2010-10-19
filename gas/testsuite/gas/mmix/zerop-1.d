#objdump: -drt

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ g     F \.text	0+ Main


Disassembly of section \.text:

0+ <Main>:
   0:	00000000 	trap 0,0,0
   4:	fd000000 	swym 0,0,0
   8:	ff000000 	trip 0,0,0
   c:	f0000000 	jmp c <Main\+0xc>
  10:	f801e240 	pop 1,57920
  14:	f8000000 	pop 0,0
