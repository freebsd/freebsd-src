#source: tls32.s
#source: tlslib32.s
#as: -a32
#ld: -melf32ppc
#objdump: -sj.got
#target: powerpc*-*-*

.*: +file format elf32-powerpc

Contents of section \.got:
 1810128 4e800021 00000000 00000000 00000000  .*
