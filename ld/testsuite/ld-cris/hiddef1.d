#source: gotrel1.s
#source: hiddef1.s
#source: hidrefgotplt1.s
#ld: -shared -m crislinux
#as: --pic --no-underscore -I$srcdir/$subdir --em=criself
#readelf: -S -s -r

# Regression test for mishandling of GOTPLT relocs against a
# hidden symbol, where the reloc is found after the symbol
# definition.  There should be no PLT, just a single GOT entry
# from a GOTPLT reloc moved to the .got section.  It's hard to
# check for absence of a .plt section, so we just check the
# number of symbols and sections.  When the number of symbols
# and sections change, make sure that there's no .plt and that
# dsofn is hidden (not exported as a dynamic symbol).

There are 11 section headers, starting at offset 0x[0-9a-f]+:
#...
  \[[ 0-9]+\] \.got              PROGBITS        [0-9a-f]+ [0-9a-f]+ 0+10 04  WA  0   0  4
#...
Relocation section '\.rela\.dyn' at offset 0x[0-9a-f]+ contains 1 entries:
#...
[0-9a-f]+  0+c R_CRIS_RELATIVE                              [0-9a-f]+
#...
Symbol table '\.dynsym' contains 6 entries:
#...
Symbol table '\.symtab' contains 16 entries:
#pass
