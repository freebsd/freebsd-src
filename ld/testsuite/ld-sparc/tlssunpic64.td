#source: tlssunpic64.s
#source: tlspic.s
#as: --64 -K PIC
#ld: -shared -melf64_sparc
#objdump: -sj.tdata
#target: sparc*-*-*

.*: +file format elf64-sparc

Contents of section .tdata:
# FIXME
#pass
