#objdump: -p
#as:	  -m68hc11
#name:	  Elf flags 68HC11 16-bit int, 64-bit double
#source:  abi.s

.*: +file format elf32\-m68hc11
private flags = 2:\[abi=16-bit int, 64-bit double, cpu=HC11\] \[memory=flat\]
