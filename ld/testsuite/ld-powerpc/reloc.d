# Test that orphan reloc sections are placed before .rela.plt even when
# .rela.plt is the only reloc section.

#source: reloc.s
#ld: -shared -z nocombreloc
#objdump: -hw

.*: +file format elf.*
#...
.*\.relaplatypus.*
#...
.*\.rela\.plt.*
#pass
