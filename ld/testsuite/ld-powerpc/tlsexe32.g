#source: tls32.s
#as: -a32
#ld: -melf32ppc tmpdir/libtlslib32.so
#objdump: -sj.got
#target: powerpc*-*-*

.*: +file format elf32-powerpc

Contents of section \.got:
 18103b8 4e800021 01810318 00000000 00000000  .*
 18103c8 00000000 00000000 00000000           .*
