#objdump: -dw
#name: x86-64 rep prefix

.*: +file format .*

Disassembly of section .text:

0+ <_start>:
   0:	f3 6c[ 	]+rep insb \(%dx\),%es:\(%rdi\)
   2:	f3 6e[ 	]+rep outsb %ds:\(%rsi\),\(%dx\)
   4:	f3 a4[ 	]+rep movsb %ds:\(%rsi\),%es:\(%rdi\)
   6:	f3 ac[ 	]+rep lods %ds:\(%rsi\),%al
   8:	f3 aa[ 	]+rep stos %al,%es:\(%rdi\)
   a:	f3 a6[ 	]+repz cmpsb %es:\(%rdi\),%ds:\(%rsi\)
   c:	f3 ae[ 	]+repz scas %es:\(%rdi\),%al
   e:	66 f3 6d[ 	]+rep insw \(%dx\),%es:\(%rdi\)
  11:	66 f3 6f[ 	]+rep outsw %ds:\(%rsi\),\(%dx\)
  14:	66 f3 a5[ 	]+rep movsw %ds:\(%rsi\),%es:\(%rdi\)
  17:	66 f3 ad[ 	]+rep lods %ds:\(%rsi\),%ax
  1a:	66 f3 ab[ 	]+rep stos %ax,%es:\(%rdi\)
  1d:	66 f3 a7[ 	]+repz cmpsw %es:\(%rdi\),%ds:\(%rsi\)
  20:	66 f3 af[ 	]+repz scas %es:\(%rdi\),%ax
  23:	f3 6d[ 	]+rep insl \(%dx\),%es:\(%rdi\)
  25:	f3 6f[ 	]+rep outsl %ds:\(%rsi\),\(%dx\)
  27:	f3 a5[ 	]+rep movsl %ds:\(%rsi\),%es:\(%rdi\)
  29:	f3 ad[ 	]+rep lods %ds:\(%rsi\),%eax
  2b:	f3 ab[ 	]+rep stos %eax,%es:\(%rdi\)
  2d:	f3 a7[ 	]+repz cmpsl %es:\(%rdi\),%ds:\(%rsi\)
  2f:	f3 af[ 	]+repz scas %es:\(%rdi\),%eax
  31:	f3 48 a5[ 	]+rep movsq %ds:\(%rsi\),%es:\(%rdi\)
  34:	f3 48 ad[ 	]+rep lods %ds:\(%rsi\),%rax
  37:	f3 48 ab[ 	]+rep stos %rax,%es:\(%rdi\)
  3a:	f3 48 a7[ 	]+repz cmpsq %es:\(%rdi\),%ds:\(%rsi\)
  3d:	f3 48 af[ 	]+repz scas %es:\(%rdi\),%rax
  40:	67 f3 6c[ 	]+rep addr32 insb \(%dx\),%es:\(%edi\)
  43:	67 f3 6e[ 	]+rep addr32 outsb %ds:\(%esi\),\(%dx\)
  46:	67 f3 a4[ 	]+rep addr32 movsb %ds:\(%esi\),%es:\(%edi\)
  49:	67 f3 ac[ 	]+rep addr32 lods %ds:\(%esi\),%al
  4c:	67 f3 aa[ 	]+rep addr32 stos %al,%es:\(%edi\)
  4f:	67 f3 a6[ 	]+repz addr32 cmpsb %es:\(%edi\),%ds:\(%esi\)
  52:	67 f3 ae[ 	]+repz addr32 scas %es:\(%edi\),%al
  55:	67 66 f3 6d[ 	]+rep addr32 insw \(%dx\),%es:\(%edi\)
  59:	67 66 f3 6f[ 	]+rep addr32 outsw %ds:\(%esi\),\(%dx\)
  5d:	67 66 f3 a5[ 	]+rep addr32 movsw %ds:\(%esi\),%es:\(%edi\)
  61:	67 66 f3 ad[ 	]+rep addr32 lods %ds:\(%esi\),%ax
  65:	67 66 f3 ab[ 	]+rep addr32 stos %ax,%es:\(%edi\)
  69:	67 66 f3 a7[ 	]+repz addr32 cmpsw %es:\(%edi\),%ds:\(%esi\)
  6d:	67 66 f3 af[ 	]+repz addr32 scas %es:\(%edi\),%ax
  71:	67 f3 6d[ 	]+rep addr32 insl \(%dx\),%es:\(%edi\)
  74:	67 f3 6f[ 	]+rep addr32 outsl %ds:\(%esi\),\(%dx\)
  77:	67 f3 a5[ 	]+rep addr32 movsl %ds:\(%esi\),%es:\(%edi\)
  7a:	67 f3 ad[ 	]+rep addr32 lods %ds:\(%esi\),%eax
  7d:	67 f3 ab[ 	]+rep addr32 stos %eax,%es:\(%edi\)
  80:	67 f3 a7[ 	]+repz addr32 cmpsl %es:\(%edi\),%ds:\(%esi\)
  83:	67 f3 af[ 	]+repz addr32 scas %es:\(%edi\),%eax
  86:	67 f3 48 a5[ 	]+rep addr32 movsq %ds:\(%esi\),%es:\(%edi\)
  8a:	67 f3 48 ad[ 	]+rep addr32 lods %ds:\(%esi\),%rax
  8e:	67 f3 48 ab[ 	]+rep addr32 stos %rax,%es:\(%edi\)
  92:	67 f3 48 a7[ 	]+repz addr32 cmpsq %es:\(%edi\),%ds:\(%esi\)
  96:	67 f3 48 af[ 	]+repz addr32 scas %es:\(%edi\),%rax
#pass
