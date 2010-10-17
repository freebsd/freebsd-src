#objdump: -p
#as:	  -m68hc11 -mshort-double
#name:	  Elf flags 68HC11 16-bit int, 32-bit double
#source:  abi.s

.*: +file format elf32\-m68hc11
private flags = 0:\[abi=16-bit int, 32-bit double, cpu=HC11\] \[memory=flat\]
