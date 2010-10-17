#source: tls.s
#source: tlslib.s
#as: -a64
#ld: -melf64ppc
#objdump: -sj.got
#target: powerpc64*-*-*

.*: +file format elf64-powerpc

Contents of section \.got:
 100101e0 00000000 100181e0 ffffffff ffff8018  .*
 100101f0 ffffffff ffff8058                    .*
