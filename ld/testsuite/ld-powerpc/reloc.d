# Test that orphan reloc sections are placed before .rela.plt even when
# .rela.plt is the only reloc section.  Also tests that orphan non-alloc
# reloc sections go after alloc sections.

#source: reloc.s
#ld: -shared -z nocombreloc
#objdump: -hw

.*: +file format elf.*
#...
.*\.relaplatypus.*
#...
.*\.rela\.plt.*
#...
.*\.relaechidna.*
#pass
