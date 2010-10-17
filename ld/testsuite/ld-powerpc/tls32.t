#source: tls32.s
#source: tlslib32.s
#as: -a32
#ld: -melf32ppc
#objdump: -sj.tdata
#target: powerpc*-*-*

.*: +file format elf32-powerpc

Contents of section \.tdata:
 1810108 12345678 23456789 3456789a 456789ab  .*
 1810118 56789abc 6789abcd 789abcde 00c0ffee  .*
