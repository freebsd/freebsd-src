#objdump: -dw
#name: i386 rep prefix

.*: +file format .*

Disassembly of section .text:

0+ <_start>:
   0:	f3 6c[ 	]+rep insb \(%dx\),%es:\(%edi\)
   2:	f3 6e[ 	]+rep outsb %ds:\(%esi\),\(%dx\)
   4:	f3 a4[ 	]+rep movsb %ds:\(%esi\),%es:\(%edi\)
   6:	f3 ac[ 	]+rep lods %ds:\(%esi\),%al
   8:	f3 aa[ 	]+rep stos %al,%es:\(%edi\)
   a:	f3 a6[ 	]+repz cmpsb %es:\(%edi\),%ds:\(%esi\)
   c:	f3 ae[ 	]+repz scas %es:\(%edi\),%al
   e:	66 f3 6d[ 	]+rep insw \(%dx\),%es:\(%edi\)
  11:	66 f3 6f[ 	]+rep outsw %ds:\(%esi\),\(%dx\)
  14:	66 f3 a5[ 	]+rep movsw %ds:\(%esi\),%es:\(%edi\)
  17:	66 f3 ad[ 	]+rep lods %ds:\(%esi\),%ax
  1a:	66 f3 ab[ 	]+rep stos %ax,%es:\(%edi\)
  1d:	66 f3 a7[ 	]+repz cmpsw %es:\(%edi\),%ds:\(%esi\)
  20:	66 f3 af[ 	]+repz scas %es:\(%edi\),%ax
  23:	f3 6d[ 	]+rep insl \(%dx\),%es:\(%edi\)
  25:	f3 6f[ 	]+rep outsl %ds:\(%esi\),\(%dx\)
  27:	f3 a5[ 	]+rep movsl %ds:\(%esi\),%es:\(%edi\)
  29:	f3 ad[ 	]+rep lods %ds:\(%esi\),%eax
  2b:	f3 ab[ 	]+rep stos %eax,%es:\(%edi\)
  2d:	f3 a7[ 	]+repz cmpsl %es:\(%edi\),%ds:\(%esi\)
  2f:	f3 af[ 	]+repz scas %es:\(%edi\),%eax
  31:	67 f3 6c[ 	]+rep addr16 insb \(%dx\),%es:\(%di\)
  34:	67 f3 6e[ 	]+rep addr16 outsb %ds:\(%si\),\(%dx\)
  37:	67 f3 a4[ 	]+rep addr16 movsb %ds:\(%si\),%es:\(%di\)
  3a:	67 f3 ac[ 	]+rep addr16 lods %ds:\(%si\),%al
  3d:	67 f3 aa[ 	]+rep addr16 stos %al,%es:\(%di\)
  40:	67 f3 a6[ 	]+repz addr16 cmpsb %es:\(%di\),%ds:\(%si\)
  43:	67 f3 ae[ 	]+repz addr16 scas %es:\(%di\),%al
  46:	67 66 f3 6d[ 	]+rep addr16 insw \(%dx\),%es:\(%di\)
  4a:	67 66 f3 6f[ 	]+rep addr16 outsw %ds:\(%si\),\(%dx\)
  4e:	67 66 f3 a5[ 	]+rep addr16 movsw %ds:\(%si\),%es:\(%di\)
  52:	67 66 f3 ad[ 	]+rep addr16 lods %ds:\(%si\),%ax
  56:	67 66 f3 ab[ 	]+rep addr16 stos %ax,%es:\(%di\)
  5a:	67 66 f3 a7[ 	]+repz addr16 cmpsw %es:\(%di\),%ds:\(%si\)
  5e:	67 66 f3 af[ 	]+repz addr16 scas %es:\(%di\),%ax
  62:	67 f3 6d[ 	]+rep addr16 insl \(%dx\),%es:\(%di\)
  65:	67 f3 6f[ 	]+rep addr16 outsl %ds:\(%si\),\(%dx\)
  68:	67 f3 a5[ 	]+rep addr16 movsl %ds:\(%si\),%es:\(%di\)
  6b:	67 f3 ad[ 	]+rep addr16 lods %ds:\(%si\),%eax
  6e:	67 f3 ab[ 	]+rep addr16 stos %eax,%es:\(%di\)
  71:	67 f3 a7[ 	]+repz addr16 cmpsl %es:\(%di\),%ds:\(%si\)
  74:	67 f3 af[ 	]+repz addr16 scas %es:\(%di\),%eax
	...
