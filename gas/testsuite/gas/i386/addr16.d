#objdump: -drw
#name: i386 16-bit addressing in 32-bit mode.

.*: +file format .*

Disassembly of section .text:

0+000 <.text>:
[	 ]*0:[	 ]+67 a0 98 08 [	 ]+addr16[	 ]+mov[ 	]+0x898,%al
[	 ]*4:[	 ]+67 66 a1 98 08 [	 ]+addr16[	 ]+mov[ 	]+0x898,%ax
[	 ]*9:[	 ]+67 a1 98 08 [	 ]+addr16[	 ]+mov[ 	]+0x898,%eax
[	 ]*d:[	 ]+67 a2 98 08 [	 ]+addr16[	 ]+mov[ 	]+%al,0x898
[	 ]*11:[	 ]+67 66 a3 98 08 [	 ]+addr16[	 ]+mov[ 	]+%ax,0x898
[	 ]*16:[	 ]+67 a3 98 08[	 ]+addr16[	 ]+mov[ 	]+%eax,0x898
#pass
