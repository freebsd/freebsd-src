#as: -J
#objdump: -drw
#name: x86-64 rip addressing

.*: +file format .*

Disassembly of section .text:

0+000 <.text>:
[	 ]*0:[	 ]+8d 05 00 00 00 00[	 ]+lea[	 ]+0x0\(%rip\),%eax[ 	]*(#.*)?
[	 ]*6:[	 ]+8d 05 11 11 11 11[	 ]+lea[	 ]+0x11111111\(%rip\),%eax[ 	]*(#.*)?
[	 ]*c:[	 ]+8d 05 01 00 00 00[	 ]+lea[	 ]+0x1\(%rip\),%eax[ 	]*(#.*)?
[	 ]*12:[	 ]+8d 05 00 00 00 00[	 ]+lea[	 ]+0x0\(%rip\),%eax[ 	]*(#.*)?
#pass
