#objdump: -dw
#name: x86-64 manual rex prefix use

.*: +file format elf64-x86-64

Disassembly of section .text:

0+ <_start>:
[	 ]*[0-9a-f]+:[	 ]+40 0f ae 00[	 ]+rex fxsavel?[	 ]+\(%rax\)
[	 ]*[0-9a-f]+:[	 ]+48 0f ae 00[	 ]+(rex64 )?fxsaveq?[	 ]+\(%rax\)
[	 ]*[0-9a-f]+:[	 ]+41 0f ae 00[	 ]+fxsavel?[	 ]+\(%r8\)
[	 ]*[0-9a-f]+:[	 ]+49 0f ae 00[	 ]+(rex64Z? )?fxsaveq?[	 ]+\(%r8\)
[	 ]*[0-9a-f]+:[	 ]+42 0f ae 04 05 00 00 00 00[	 ]+fxsavel?[	 ]+(0x0)?\(,%r8(,1)?\)
[	 ]*[0-9a-f]+:[	 ]+4a 0f ae 04 05 00 00 00 00[	 ]+(rex64Y? )?fxsaveq?[	 ]+(0x0)?\(,%r8(,1)?\)
[	 ]*[0-9a-f]+:[	 ]+43 0f ae 04 00[	 ]+fxsavel?[	 ]+\(%r8,%r8(,1)?\)
[	 ]*[0-9a-f]+:[	 ]+4b 0f ae 04 00[	 ]+(rex64(YZ)? )?fxsaveq?[	 ]+\(%r8,%r8(,1)?\)
#pass
