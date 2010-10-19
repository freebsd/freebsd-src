#as: -J
#objdump: -drw
#name: x86-64 rip addressing

.*: +file format elf64-x86-64

Disassembly of section .text:

0+000 <.text>:
[	 ]*0:[	 ]+8d 05 00 00 00 00[	 ]+lea[	 ]+0\(%rip\),%eax[ 	]*(#.*)?
[	 ]*6:[	 ]+8d 05 11 11 11 11[	 ]+lea[	 ]+286331153\(%rip\),%eax[ 	]*(#.*)?
[	 ]*c:[	 ]+8d 05 01 00 00 00[	 ]+lea[	 ]+1\(%rip\),%eax[ 	]*(#.*)?
[	 ]*12:[	 ]+8d 05 00 00 00 00[	 ]+lea[	 ]+0\(%rip\),%eax[ 	]*(#.*)?
