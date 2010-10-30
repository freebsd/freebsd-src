#source: ldr1.s
#source: ldr2.s
#as: -little
#ld: -r -EL
#readelf: -r -x1 -x2
#target: sh*-*-elf sh*-*-linux*
#notarget: sh64*-*-linux*

# Make sure relocations against global and local symbols with relative and
# absolute 32-bit relocs don't come out wrong after ld -r.  Remember that
# SH uses partial_inplace (sort-of REL within RELA) with its confusion
# where and which addends to use and how.  A file linked -r must have the
# same layout as a plain assembly file: the addend is in the data only.

Relocation section '\.rela\.text' at offset 0x[0-9a-f]+ contains 1 entries:
.*
00000008  00000101 R_SH_DIR32 +00000000 +\.text +\+ 0

Hex dump of section '\.text':
.*
  0x00000000 09000900 09000900 0c000000 .*

Hex dump of section '\.rela\.text':
  0x00000000 08000000 01010000 00000000 .*
