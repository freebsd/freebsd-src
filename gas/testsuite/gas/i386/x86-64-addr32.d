#as: -J
#objdump: -drw
#name: x86-64 32-bit addressing

.*: +file format elf64-x86-64

Disassembly of section .text:

0+000 <.text>:
[	 ]*0:[	 ]+67 48 8d 80 00 00 00 00[	 ]+addr32[	 ]+lea[ 	]+0x0\(%[re]ax\),%rax.*
[	 ]*8:[	 ]+67 49 8d 80 00 00 00 00[	 ]+addr32[	 ]+lea[ 	]+0x0\(%r8d?\),%rax.*
[	 ]*10:[	 ]+67 48 8d 05 00 00 00 00[	 ]+addr32[	 ]+lea[ 	]+0\(%[re]ip\),%rax.*
[	 ]*18:[	 ]+67 48 8d 04 25 00 00 00 00[	 ]+addr32[	 ]+lea[ 	]+0x0,%rax.*
