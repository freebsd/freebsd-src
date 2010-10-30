#objdump: -dwMsuffix
#name: i386 rep prefix (with suffixes)

.*: +file format .*

Disassembly of section .text:

0+000 <_start>:
   0:	f3 ac[ 	]+rep lodsb %ds:\(%esi\),%al
   2:	f3 aa[ 	]+rep stosb %al,%es:\(%edi\)
   4:	66 f3 ad[ 	]+rep lodsw %ds:\(%esi\),%ax
   7:	66 f3 ab[ 	]+rep stosw %ax,%es:\(%edi\)
   a:	f3 ad[ 	]+rep lodsl %ds:\(%esi\),%eax
   c:	f3 ab[ 	]+rep stosl %eax,%es:\(%edi\)
#pass
