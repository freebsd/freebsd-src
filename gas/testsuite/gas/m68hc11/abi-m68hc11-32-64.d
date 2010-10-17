#objdump: -p
#as:	  -m68hc11 -mlong
#name:	  Elf flags 68HC11 32-bit int, 64-bit double
#source:  abi.s

.*: +file format elf32\-m68hc11
private flags = 3:\[abi=32-bit int, 64-bit double, cpu=HC11\] \[memory=flat\]
