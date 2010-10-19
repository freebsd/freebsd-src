#objdump: -r
#source: mixed-mode-reloc.s
#name: x86 mixed mode relocs (32-bit object)

.*: +file format .*i386.*

RELOCATION RECORDS FOR \[.text\]:
OFFSET[ 	]+TYPE[ 	]+VALUE[ 	]*
[0-9a-f]+[ 	]+R_386_GOT32[ 	]+xtrn[ 	]*
[0-9a-f]+[ 	]+R_386_PLT32[ 	]+xtrn[ 	]*
[0-9a-f]+[ 	]+R_386_GOT32[ 	]+xtrn[ 	]*
[0-9a-f]+[ 	]+R_386_PLT32[ 	]+xtrn[ 	]*
[0-9a-f]+[ 	]+R_386_GOT32[ 	]+xtrn[ 	]*
[0-9a-f]+[ 	]+R_386_PLT32[ 	]+xtrn[ 	]*
