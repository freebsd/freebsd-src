#source: dso-1.s
#source: dso-2.s
#as: --pic --no-underscore
#ld: --shared -m crislinux --version-script $srcdir/$subdir/hide1
#readelf: -S -s -r

# Use "dsofn" from dso-1 in a GOTPLT reloc, but hide it in a
# version script.  This will change the incoming GOTPLT reloc to
# instead be a (local) GOT reloc.  There are no other .rela.got
# entries.  This formerly SEGV:ed because .rela.got was created
# too late to have it mapped to an output section.

There are 15 section headers.*
#...
  \[ 1\] \.hash             HASH     .*
  \[ 2\] \.dynsym           DYNSYM   .*
  \[ 3\] \.dynstr           STRTAB   .*
  \[ 4\] \.gnu\.version      VERSYM  .*
  \[ 5\] \.gnu\.version_d    VERDEF  .*
  \[ 6\] \.rela\.dyn         RELA    .*
  \[ 7\] \.text             PROGBITS .*
  \[ 8\] \.data             PROGBITS .*
  \[ 9\] \.dynamic          DYNAMIC  .*
  \[10\] \.got              PROGBITS .*
  \[11\] \.bss              NOBITS   .*
  \[12\] \.shstrtab         STRTAB   .*
  \[13\] \.symtab           SYMTAB   .*
  \[14\] \.strtab           STRTAB   .*
#...
Relocation section '\.rela\.dyn' at offset 0x[0-9a-f]+ contains 1 entries:
#...
000022f4  0000000c R_CRIS_RELATIVE                              00000234
#...
Symbol table '\.dynsym' contains 14 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 0+     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    1 
     2: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    2 
     3: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    3 
     4: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    4 
     5: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    5 
     6: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    6 
     7: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    7 
     8: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    8 
     9: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    9 
    10: [0-9a-f]+     0 SECTION LOCAL  DEFAULT   10 
    11: [0-9a-f]+     0 SECTION LOCAL  DEFAULT   11 
    12: 0+     0 OBJECT  GLOBAL DEFAULT  ABS TST1
    13: 0+238     0 FUNC    GLOBAL DEFAULT    7 export_1@@TST1

Symbol table '\.symtab' contains 23 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 0+     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    1 
     2: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    2 
     3: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    3 
     4: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    4 
     5: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    5 
     6: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    6 
     7: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    7 
     8: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    8 
     9: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    9 
    10: [0-9a-f]+     0 SECTION LOCAL  DEFAULT   10 
    11: [0-9a-f]+     0 SECTION LOCAL  DEFAULT   11 
    12: [0-9a-f]+     0 SECTION LOCAL  DEFAULT   12 
    13: [0-9a-f]+     0 SECTION LOCAL  DEFAULT   13 
    14: [0-9a-f]+     0 SECTION LOCAL  DEFAULT   14 
    15: 0+2260     0 OBJECT  LOCAL  DEFAULT  ABS _DYNAMIC
    16: 0+22f8     0 NOTYPE  LOCAL  DEFAULT  ABS __bss_start
    17: 0+22f8     0 NOTYPE  LOCAL  DEFAULT  ABS _edata
    18: 0+22e8     0 OBJECT  LOCAL  DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
    19: 0+2300     0 NOTYPE  LOCAL  DEFAULT  ABS _end
    20: 0+234     0 FUNC    LOCAL  DEFAULT    7 dsofn
    21: 0+     0 OBJECT  GLOBAL DEFAULT  ABS TST1
    22: 0+238     0 FUNC    GLOBAL DEFAULT    7 export_1
