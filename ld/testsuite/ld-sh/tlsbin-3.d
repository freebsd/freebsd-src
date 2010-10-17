#source: tlsbinpic.s
#source: tlsbin.s
#as: -little
#ld: -EL tmpdir/tlsbin-0-dso.so
#objdump: -sj.got
#target: sh*-*-linux* sh*-*-netbsd*

.*: +file format elf32-sh.*

Contents of section \.got:
 4130b8 [0-9a-f]+ 00000000 00000000 [0-9a-f]+  .*
 4130c8 00000000 00000000 00000000 00000000  .*
