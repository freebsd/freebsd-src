#source: dso-1.s
#as: --pic --no-underscore --march=v32 --em=criself
#ld: --shared -m crislinux
#objdump: -p -h

# Sanity check; just an empty GOT.

.*:     file format elf32-cris

Program Header:
    LOAD off    0x0+ vaddr 0x0+ paddr 0x0+ align 2\*\*13
         filesz 0x0+144 memsz 0x0+144 flags r-x
    LOAD off    0x0+144 vaddr 0x0+2144 paddr 0x0+2144 align 2\*\*13
         filesz 0x0+64 memsz 0x0+64 flags rw-
 DYNAMIC off    0x0+144 vaddr 0x0+2144 paddr 0x0+2144 align 2\*\*2
         filesz 0x0+58 memsz 0x0+58 flags rw-
Dynamic Section:
  HASH        0x94
  STRTAB      0x120
  SYMTAB      0xc0
  STRSZ       0x1f
  SYMENT      0x10
private flags = 2: \[v32\]
Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 \.hash         0+2c  0+94  0+94  0+94  2\*\*2
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  1 \.dynsym       0+60  0+c0  0+c0  0+c0  2\*\*2
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  2 \.dynstr       0+1f  0+120  0+120  0+120  2\*\*0
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  3 \.text         0+4  0+140  0+140  0+140  2\*\*1
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  4 \.dynamic      0+58  0+2144  0+2144  0+144  2\*\*2
                  CONTENTS, ALLOC, LOAD, DATA
  5 \.got          0+c  0+219c  0+219c  0+19c  2\*\*2
                  CONTENTS, ALLOC, LOAD, DATA
