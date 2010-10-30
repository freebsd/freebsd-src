#objdump: -dw
#name: x86-64 amdfam10

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
   0:	f3 48 0f bd 19[ 	]+lzcnt  \(%rcx\),%rbx
   5:	f3 0f bd 19[ 	]+lzcnt  \(%rcx\),%ebx
   9:	66 f3 0f bd 19[ 	]+lzcnt  \(%rcx\),%bx
   e:	f3 48 0f bd d9[ 	]+lzcnt  %rcx,%rbx
  13:	f3 0f bd d9[ 	]+lzcnt  %ecx,%ebx
  17:	66 f3 0f bd d9[ 	]+lzcnt  %cx,%bx
  1c:	f3 48 0f b8 19[ 	]+popcnt \(%rcx\),%rbx
  21:	f3 0f b8 19[ 	]+popcnt \(%rcx\),%ebx
  25:	66 f3 0f b8 19[ 	]+popcnt \(%rcx\),%bx
  2a:	f3 48 0f b8 d9[ 	]+popcnt %rcx,%rbx
  2f:	f3 0f b8 d9[ 	]+popcnt %ecx,%ebx
  33:	66 f3 0f b8 d9[ 	]+popcnt %cx,%bx
  38:	66 0f 79 ca[ 	]+extrq  %xmm2,%xmm1
  3c:	66 0f 78 c1 02 04[ 	]+extrq  \$0x4,\$0x2,%xmm1
  42:	f2 0f 79 ca[ 	]+insertq %xmm2,%xmm1
  46:	f2 0f 78 ca 02 04[ 	]+insertq \$0x4,\$0x2,%xmm2,%xmm1
  4c:	f2 0f 2b 09[ 	]+movntsd %xmm1,\(%rcx\)
  50:	f3 0f 2b 09[ 	]+movntss %xmm1,\(%rcx\)
	...
