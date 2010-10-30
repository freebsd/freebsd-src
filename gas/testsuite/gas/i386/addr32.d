#objdump: -drw -mi8086
#name: i386 32-bit addressing in 16-bit mode.

.*: +file format .*

Disassembly of section .text:

0+000 <.text>:
[	 ]*0:[	 ]+67 a0 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+0x600898,%al
[	 ]*6:[	 ]+67 a1 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+0x600898,%ax
[	 ]*c:[	 ]+67 66 a1 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+0x600898,%eax
[	 ]*13:[	 ]+67 a2 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+%al,0x600898
[	 ]*19:[	 ]+67 a3 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+%ax,0x600898
[	 ]*1f:[	 ]+67 66 a3 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+%eax,0x600898
#pass
