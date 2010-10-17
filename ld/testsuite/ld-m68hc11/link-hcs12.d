#source: link-hcs12.s -m68hcs12
#source: link-hc12.s -m68hc12
#as: -mshort
#ld: -m m68hc12elf
#objdump: -p -d --prefix-addresses -r
#target: m6811-*-* m6812-*-*

.*:     file format elf32\-m68hc12

Program Header:
    LOAD off    0x0+ vaddr 0x0+1000 paddr 0x0+1000 align 2\*\*12
         filesz 0x0+100 memsz 0x0+100 flags rw-
    LOAD off    0x0+1000 vaddr 0x0+8000 paddr 0x0+8000 align 2\*\*12
         filesz 0x0+6 memsz 0x0+6 flags r-x
    LOAD off    0x0+1100 vaddr 0x0+1100 paddr 0x0+8006 align 2\*\*12
         filesz 0x0+ memsz 0x0+ flags rw-
private flags = 22:\[abi=16\-bit int, 64\-bit double, cpu=HCS12\] \[memory=flat\]

Disassembly of section .text:
0+8000 <_start> jsr	0+8005 <main>
0+8003 <_start\+0x3> bra	0+8000 <_start>
0+8005 <main> rts


