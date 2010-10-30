#as: -J
#objdump: -drwMintel
#name: x86-64 rip addressing (Intel mode)
#source: x86-64-rip.s

.*: +file format .*

Disassembly of section .text:

0+000 <.text>:
[	 ]*0:[	 ]+8d 05 00 00 00 00[	 ]+lea[	 ]+eax,\[rip\+0x0\][ 	]*(#.*)?
[	 ]*6:[	 ]+8d 05 11 11 11 11[	 ]+lea[	 ]+eax,\[rip\+0x11111111\][ 	]*(#.*)?
[	 ]*c:[	 ]+8d 05 01 00 00 00[	 ]+lea[	 ]+eax,\[rip\+0x1\][ 	]*(#.*)?
[	 ]*12:[	 ]+8d 05 00 00 00 00[	 ]+lea[	 ]+eax,\[rip\+0x0\][ 	]*(#.*)?
#pass
