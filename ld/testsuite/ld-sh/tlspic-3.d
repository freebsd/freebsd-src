#source: tlspic1.s
#source: tlspic2.s
#as: -little
#ld: -shared -EL
#objdump: -sj.got
#target: sh*-*-linux* sh*-*-netbsd*

.*: +file format elf32-sh.*

Contents of section \.got:
 [0-9a-f]+ [0-9a-f]+ 00000000 00000000 [0-9a-f]+  .*
 [0-9a-f]+ 00000000 08000000 00000000 00000000  .*
 [0-9a-f]+ 00000000 00000000 18000000 00000000  .*
 [0-9a-f]+ 00000000 00000000 00000000 00000000  .*
 [0-9a-f]+ 10000000 00000000 +.*
