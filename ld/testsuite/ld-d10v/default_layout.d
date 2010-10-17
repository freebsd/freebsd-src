#source: simple.s
#ld: 
#objdump: -h

.*:     file format elf32-d10v

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 \.text         00000004  01014000  01014000  00001000  2\*\*0
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  1 \.data         00000000  02000004  02000004  00001004  2\*\*0
                  CONTENTS, ALLOC, LOAD, DATA
  2 \.bss          00000000  02000004  02000004  00001004  2\*\*0
                  ALLOC
  3 .stack        00000000  0200bffe  0200bffe  00001004  2\*\*0
                  CONTENTS
