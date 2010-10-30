#objdump: -dw
#name: x86-64 stack-related opcodes

.*: +file format .*

Disassembly of section .text:

0+ <_start>:
[	 ]*[0-9a-f]+:[	 ]+50[	 ]+pushq?[	 ]+%rax
[	 ]*[0-9a-f]+:[	 ]+66 50[	 ]+pushw?[	 ]+%ax
[	 ]*[0-9a-f]+:[	 ]+66 48 50[	 ]+pushq?[	 ]+%rax
[	 ]*[0-9a-f]+:[	 ]+58[	 ]+popq?[	 ]+%rax
[	 ]*[0-9a-f]+:[	 ]+66 58[	 ]+popw?[	 ]+%ax
[	 ]*[0-9a-f]+:[	 ]+66 48 58[	 ]+popq?[	 ]+%rax
[	 ]*[0-9a-f]+:[	 ]+8f c0[	 ]+popq?[	 ]+%rax
[	 ]*[0-9a-f]+:[	 ]+66 8f c0[	 ]+popw?[	 ]+%ax
[	 ]*[0-9a-f]+:[	 ]+66 48 8f c0[	 ]+popq?[	 ]+%rax
[	 ]*[0-9a-f]+:[	 ]+8f 00[	 ]+popq[	 ]+\(%rax\)
[	 ]*[0-9a-f]+:[	 ]+66 8f 00[	 ]+popw[	 ]+\(%rax\)
[	 ]*[0-9a-f]+:[	 ]+66 48 8f 00[	 ]+popq[	 ]+\(%rax\)
[	 ]*[0-9a-f]+:[	 ]+ff d0[	 ]+callq?[	 ]+\*%rax
[	 ]*[0-9a-f]+:[	 ]+66 ff d0[	 ]+callw?[	 ]+\*%ax
[	 ]*[0-9a-f]+:[	 ]+66 48 ff d0[	 ]+callq?[	 ]+\*%rax
[	 ]*[0-9a-f]+:[	 ]+ff 10[	 ]+callq[	 ]+\*\(%rax\)
[	 ]*[0-9a-f]+:[	 ]+66 ff 10[	 ]+callw[	 ]+\*\(%rax\)
[	 ]*[0-9a-f]+:[	 ]+66 48 ff 10[	 ]+callq[	 ]+\*\(%rax\)
[	 ]*[0-9a-f]+:[	 ]+ff e0[	 ]+jmpq?[	 ]+\*%rax
[	 ]*[0-9a-f]+:[	 ]+66 ff e0[	 ]+jmpw?[	 ]+\*%ax
[	 ]*[0-9a-f]+:[	 ]+66 48 ff e0[	 ]+jmpq?[	 ]+\*%rax
[	 ]*[0-9a-f]+:[	 ]+ff 20[	 ]+jmpq[	 ]+\*\(%rax\)
[	 ]*[0-9a-f]+:[	 ]+66 ff 20[	 ]+jmpw[	 ]+\*\(%rax\)
[	 ]*[0-9a-f]+:[	 ]+66 48 ff 20[	 ]+jmpq[	 ]+\*\(%rax\)
[	 ]*[0-9a-f]+:[	 ]+ff f0[	 ]+pushq?[	 ]+%rax
[	 ]*[0-9a-f]+:[	 ]+66 ff f0[	 ]+pushw?[	 ]+%ax
[	 ]*[0-9a-f]+:[	 ]+66 48 ff f0[	 ]+pushq?[	 ]+%rax
[	 ]*[0-9a-f]+:[	 ]+ff 30[	 ]+pushq[	 ]+\(%rax\)
[	 ]*[0-9a-f]+:[	 ]+66 ff 30[	 ]+pushw[	 ]+\(%rax\)
[	 ]*[0-9a-f]+:[	 ]+66 48 ff 30[	 ]+pushq[	 ]+\(%rax\)
#pass
