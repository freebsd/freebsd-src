#source: tlslib.s
#source: tlstoc.s
#as: -a64
#ld: -melf64ppc
#objdump: -sj.tdata
#target: powerpc64*-*-*

.*: +file format elf64-powerpc

Contents of section \.tdata:
 10010148 00c0ffee 00000000 12345678 9abcdef0  .*
 10010158 23456789 abcdef01 3456789a bcdef012  .*
 10010168 456789ab cdef0123 56789abc def01234  .*
 10010178 6789abcd ef012345 789abcde f0123456  .*
