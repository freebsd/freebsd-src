#source: refdbglib.s
#as: -little
#ld: -shared -EL
#objdump: -drj.text
#target: sh*-*-linux* sh*-*-netbsd*

.*: +file format elf32-sh.*

#pass
