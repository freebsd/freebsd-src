#objdump: -dw
#name: i386 amdfam10

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
   0:	f3 0f bd 19[ 	]+lzcnt  \(%ecx\),%ebx
   4:	66 f3 0f bd 19[ 	 ]+lzcnt  \(%ecx\),%bx
   9:	f3 0f bd d9[ 	 ]+lzcnt  %ecx,%ebx
   d:	66 f3 0f bd d9[ 	 ]+lzcnt  %cx,%bx
  12:	f3 0f b8 19[ 	]+popcnt \(%ecx\),%ebx
  16:	66 f3 0f b8 19[ 	]+popcnt \(%ecx\),%bx
  1b:	f3 0f b8 d9[ 	]+popcnt %ecx,%ebx
  1f:	66 f3 0f b8 d9[ 	]+popcnt %cx,%bx
  24:	66 0f 79 ca[ 	]+extrq  %xmm2,%xmm1
  28:	66 0f 78 c1 02 04[ 	]*extrq  \$0x4,\$0x2,%xmm1
  2e:	f2 0f 79 ca[ 	]+insertq %xmm2,%xmm1
  32:	f2 0f 78 ca 02 04[ 	]*insertq \$0x4,\$0x2,%xmm2,%xmm1
  38:	f2 0f 2b 09[ 	]+movntsd %xmm1,\(%ecx\)
  3c:	f3 0f 2b 09[ 	]+movntss %xmm1,\(%ecx\)

