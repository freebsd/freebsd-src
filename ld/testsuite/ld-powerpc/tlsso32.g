#source: tls32.s
#as: -a32
#ld: -shared -melf32ppc
#objdump: -sj.got
#target: powerpc*-*-*

.*: +file format elf32-powerpc

Contents of section \.got:
.* 00000000 00000000 00000000 00000000  .*
.* 00000000 00000000 00000000 00000000  .*
.* 00000000 4e800021 00010438 00000000  .*
.* 00000000                             .*
