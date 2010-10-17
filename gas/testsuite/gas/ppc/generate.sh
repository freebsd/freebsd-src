#! /bin/sh

m4 -DELF32 test1elf.asm >test1elf32.s
m4 -DELF64 test1elf.asm >test1elf64.s
m4 -DXCOFF32 test1xcoff.asm >test1xcoff32.s
#m4 -DXCOFF64 test1xcoff.asm >test1xcoff64.s
