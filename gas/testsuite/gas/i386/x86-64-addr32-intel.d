#as: -J
#objdump: -drwMintel
#name: x86-64 32-bit addressing (Intel mode)
#source: x86-64-addr32.s

.*: +file format .*

Disassembly of section .text:

0+000 <.text>:
[	 ]*0:[	 ]+67 48 8d 80 00 00 00 00[	 ]+addr32[	 ]+lea[ 	]+rax,\[[re]ax\+(0x)?0\].*
[	 ]*8:[	 ]+67 49 8d 80 00 00 00 00[	 ]+addr32[	 ]+lea[ 	]+rax,\[r8d?\+(0x)?0\].*
[	 ]*10:[	 ]+67 48 8d 05 00 00 00 00[	 ]+addr32[	 ]+lea[ 	]+rax,\[[re]ip\+(0x)?0\].*
[	 ]*18:[	 ]+67 48 8d 04 25 00 00 00 00[	 ]+addr32[	 ]+lea[ 	]+rax,ds:0x0.*
[	 ]*21:[	 ]+67 a0 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+al,ds:0x600898
[	 ]*27:[	 ]+67 66 a1 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+ax,ds:0x600898
[	 ]*2e:[	 ]+67 a1 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+eax,ds:0x600898
[	 ]*34:[	 ]+67 48 a1 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+rax,ds:0x600898
[	 ]*3b:[	 ]+67 a2 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+ds:0x600898,al
[	 ]*41:[	 ]+67 66 a3 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+ds:0x600898,ax
[	 ]*48:[	 ]+67 a3 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+ds:0x600898,eax
[	 ]*4e:[	 ]+67 48 a3 98 08 60 00[	 ]+addr32[	 ]+mov[ 	]+ds:0x600898,rax
#pass
