#source: tls.s
#as: -a64
#ld: -melf64ppc tmpdir/libtlslib.so
#objdump: -sj.got
#target: powerpc64*-*-*

.*: +file format elf64-powerpc

Contents of section \.got:
 10010610 00000000 10018610 ffffffff ffff8018  .*
 10010620 00000000 00000000 00000000 00000000  .*
 10010630 00000000 00000000 00000000 00000000  .*
