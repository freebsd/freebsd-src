# Test that orphan reloc sections are placed before .rel.plt even when
# .rel.plt is the only reloc section.
#source: reloc.s
#as: --32
#ld: -shared -melf_i386 -z nocombreloc
#objdump: -hw
#target: i?86-*-*

.*: +file format elf32-i386
#...
.*\.relplatypus.*
#...
.*\.rel\.plt.*
# x86 ld doesn't output non-alloc reloc sections to shared libs, so disable
# the following two lines for the time being.
# #...
# .*\.relechidna.*
#pass
