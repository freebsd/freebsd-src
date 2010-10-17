#source: tlsbinpic.s
#source: tlsbin.s
#as: -little
#ld: -EL tmpdir/tlsbin-0-dso.so
#objdump: -sj.tdata
#target: sh*-*-linux* sh*-*-netbsd*

.*: +file format elf32-sh.*

Contents of section .tdata:
 413000 11000000 12000000 41000000 42000000  .*
 413010 01010000 02010000 +.*
