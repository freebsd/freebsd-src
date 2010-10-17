#source: tls32.s
#as: -a32
#ld: -shared -melf32ppc
#objdump: -sj.tdata
#target: powerpc*-*-*

.*: +file format elf32-powerpc

Contents of section \.tdata:
 105a8 12345678 23456789 3456789a 456789ab  .*
 105b8 56789abc 6789abcd 789abcde           .*
