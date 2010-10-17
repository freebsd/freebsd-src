#objdump: -p
#as:	  -m68hc11 -mlong -mshort-double
#name:	  Elf flags 68HC11 32-bit int, 32-bit double
#source:  abi.s

.*: +file format elf32\-m68hc11
private flags = 1:\[abi=32-bit int, 32-bit double, cpu=HC11\] \[memory=flat\]
