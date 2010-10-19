#source: tlsbinpic.s
#source: tlsbin.s
#as: -little
#ld: -EL tmpdir/tlsbin-0-dso.so
#objdump: -sj.tdata
#target: sh*-*-linux* sh*-*-netbsd*

.*: +file format elf32-sh.*

Contents of section .tdata:
 [0-9a-f]+ 11000000 12000000 41000000 42000000  .*
 [0-9a-f]+ 01010000 02010000 +.*
