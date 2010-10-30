#as: -J
#objdump: -drw
#name: x86-64 indirect branch

.*: +file format .*

Disassembly of section .text:

0+000 <.text>:
[	 ]*0:[	 ]+ff d0[	 ]+callq[	 ]+\*%rax
[	 ]*2:[	 ]+ff d0[	 ]+callq[	 ]+\*%rax
[	 ]*4:[	 ]+ff e0[	 ]+jmpq[	 ]+\*%rax
[	 ]*6:[	 ]+ff e0[	 ]+jmpq[	 ]+\*%rax
#pass
