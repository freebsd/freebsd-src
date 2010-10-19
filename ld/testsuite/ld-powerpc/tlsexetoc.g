#source: tlstoc.s
#as: -a64
#ld: -melf64ppc tmpdir/libtlslib.so
#objdump: -sj.got
#target: powerpc64*-*-*

.*: +file format elf64-powerpc

Contents of section \.got:
.* 00000000 10018570 00000000 00000000  .*
.* 00000000 00000000 00000000 00000000  .*
.* 00000000 00000000 00000000 00000001  .*
.* 00000000 00000000 00000000 00000001  .*
.* 00000000 00000000 ffffffff ffff8050  .*
.* 00000000 00000000                    .*
