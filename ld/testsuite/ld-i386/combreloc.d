# Test that orphan reloc sections are merged into .rel.dyn with
# -z combreloc.
#source: combreloc.s
#as: --32
#ld: -shared -melf_i386 -z combreloc
#readelf: -r
#target: i?86-*-*

Relocation section '.rel.dyn' at offset 0x[0-9a-f]+ contains 4 entries:
 Offset     Info    Type            Sym.Value  Sym. Name
[0-9a-f]+  [0-9a-f]+06 R_386_GLOB_DAT    [0-9a-f]+   _start
[0-9a-f]+  [0-9a-f]+01 R_386_32          [0-9a-f]+   _start
[0-9a-f]+  [0-9a-f]+01 R_386_32          [0-9a-f]+   _start
[0-9a-f]+  [0-9a-f]+01 R_386_32          [0-9a-f]+   _start

Relocation section '.rel.plt' at offset 0x[0-9a-f]+ contains 1 entries:
 Offset     Info    Type            Sym.Value  Sym. Name
[0-9a-f]+  [0-9a-f]+07 R_386_JUMP_SLOT   [0-9a-f]+   foo
