#source: refdbg.s
#as: -little
#ld: -EL tmpdir/refdbg-0-dso.so
#objdump: -sj.debug_info
#target: sh*-*-linux* sh*-*-netbsd*

.*: +file format elf32-sh.*

Contents of section \.debug_info:
 0+0 0+0 +.*
