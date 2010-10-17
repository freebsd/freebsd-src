#source: tls.s
#as: -a64
#ld: -shared -melf64ppc
#objdump: -sj.tdata
#target: powerpc64*-*-*

.*: +file format elf64-powerpc

Contents of section \.tdata:
.* 12345678 9abcdef0 23456789 abcdef01  .*
.* 3456789a bcdef012 456789ab cdef0123  .*
.* 56789abc def01234 6789abcd ef012345  .*
.* 789abcde f0123456                    .*
