#objdump: -dwMintel
#name: x86-64 stack-related opcodes (Intel mode)
#source: x86-64-stack.s

.*: +file format .*

Disassembly of section .text:

0+ <_start>:
[	 ]*[0-9a-f]+:[	 ]+50[	 ]+push[	 ]+rax
[	 ]*[0-9a-f]+:[	 ]+66 50[	 ]+push[	 ]+ax
[	 ]*[0-9a-f]+:[	 ]+66 48 50[	 ]+push[	 ]+rax
[	 ]*[0-9a-f]+:[	 ]+58[	 ]+pop[	 ]+rax
[	 ]*[0-9a-f]+:[	 ]+66 58[	 ]+pop[	 ]+ax
[	 ]*[0-9a-f]+:[	 ]+66 48 58[	 ]+pop[	 ]+rax
[	 ]*[0-9a-f]+:[	 ]+8f c0[	 ]+pop[	 ]+rax
[	 ]*[0-9a-f]+:[	 ]+66 8f c0[	 ]+pop[	 ]+ax
[	 ]*[0-9a-f]+:[	 ]+66 48 8f c0[	 ]+pop[	 ]+rax
[	 ]*[0-9a-f]+:[	 ]+8f 00[	 ]+pop[	 ]+QWORD PTR \[rax\]
[	 ]*[0-9a-f]+:[	 ]+66 8f 00[	 ]+pop[	 ]+WORD PTR \[rax\]
[	 ]*[0-9a-f]+:[	 ]+66 48 8f 00[	 ]+pop[	 ]+QWORD PTR \[rax\]
[	 ]*[0-9a-f]+:[	 ]+ff d0[	 ]+call[	 ]+rax
[	 ]*[0-9a-f]+:[	 ]+66 ff d0[	 ]+call[	 ]+ax
[	 ]*[0-9a-f]+:[	 ]+66 48 ff d0[	 ]+call[	 ]+rax
[	 ]*[0-9a-f]+:[	 ]+ff 10[	 ]+call[	 ]+QWORD PTR \[rax\]
[	 ]*[0-9a-f]+:[	 ]+66 ff 10[	 ]+call[	 ]+WORD PTR \[rax\]
[	 ]*[0-9a-f]+:[	 ]+66 48 ff 10[	 ]+call[	 ]+QWORD PTR \[rax\]
[	 ]*[0-9a-f]+:[	 ]+ff e0[	 ]+jmp[	 ]+rax
[	 ]*[0-9a-f]+:[	 ]+66 ff e0[	 ]+jmp[	 ]+ax
[	 ]*[0-9a-f]+:[	 ]+66 48 ff e0[	 ]+jmp[	 ]+rax
[	 ]*[0-9a-f]+:[	 ]+ff 20[	 ]+jmp[	 ]+QWORD PTR \[rax\]
[	 ]*[0-9a-f]+:[	 ]+66 ff 20[	 ]+jmp[	 ]+WORD PTR \[rax\]
[	 ]*[0-9a-f]+:[	 ]+66 48 ff 20[	 ]+jmp[	 ]+QWORD PTR \[rax\]
[	 ]*[0-9a-f]+:[	 ]+ff f0[	 ]+push[	 ]+rax
[	 ]*[0-9a-f]+:[	 ]+66 ff f0[	 ]+push[	 ]+ax
[	 ]*[0-9a-f]+:[	 ]+66 48 ff f0[	 ]+push[	 ]+rax
[	 ]*[0-9a-f]+:[	 ]+ff 30[	 ]+push[	 ]+QWORD PTR \[rax\]
[	 ]*[0-9a-f]+:[	 ]+66 ff 30[	 ]+push[	 ]+WORD PTR \[rax\]
[	 ]*[0-9a-f]+:[	 ]+66 48 ff 30[	 ]+push[	 ]+QWORD PTR \[rax\]
#pass
