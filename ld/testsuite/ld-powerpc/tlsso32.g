#source: tls32.s
#as: -a32
#ld: -shared -melf32ppc
#objdump: -sj.got
#target: powerpc*-*-*

.*: +file format elf32-powerpc

Contents of section \.got:
 10664 4e800021 000105c4 00000000 00000000  .*
 10674 00000000 00000000 00000000 00000000  .*
 10684 00000000 00000000 00000000 00000000  .*
 10694 00000000                             .*
