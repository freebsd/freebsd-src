#objdump: -drw
#name: i386 nops

.*: +file format .*

Disassembly of section .text:

0+000 <.text>:
[	 ]*0:[	 ]+0f 1f 00[	 ]+nopl[ 	]+\(%eax\)
[	 ]*3:[	 ]+0f 1f 40 00[	 ]+nopl[ 	]+0x0\(%eax\)
[	 ]*7:[	 ]+0f 1f 44 00 00[	 ]+nopl[ 	]+0x0\(%eax,%eax,1\)
[	 ]*c:[	 ]+66 0f 1f 44 00 00[	 ]+nopw[ 	]+0x0\(%eax,%eax,1\)
[	 ]*12:[	 ]+0f 1f 80 00 00 00 00[	 ]+nopl[ 	]+0x0\(%eax\)
[	 ]*19:[	 ]+0f 1f 84 00 00 00 00 00[	 ]+nopl[ 	]+0x0\(%eax,%eax,1\)
[	 ]*21:[	 ]+66 0f 1f 84 00 00 00 00 00[	 ]+nopw[ 	]+0x0\(%eax,%eax,1\)
[	 ]*2a:[	 ]+66 2e 0f 1f 84 00 00 00 00 00[	 ]+nopw[ 	]+%cs:0x0\(%eax,%eax,1\)
[	 ]*34:[	 ]+0f 1f 00[	 ]+nopl[ 	]+\(%eax\)
[	 ]*37:[	 ]+0f 1f c0[	 ]+nop[ 	]+%eax
[	 ]*3a:[	 ]+66 0f 1f c0[	 ]+nop[ 	]+%ax
[	 ]*3e:[	 ]+0f 1f 00[	 ]+nopl[ 	]+\(%eax\)
[	 ]*41:[	 ]+66 0f 1f 00[	 ]+nopw[ 	]+\(%eax\)
[	 ]*45:[	 ]+0f 1f c0[	 ]+nop[ 	]+%eax
[	 ]*48:[	 ]+66 0f 1f c0[	 ]+nop[ 	]+%ax
#pass
