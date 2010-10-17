#objdump: -dw
#name: i386 amd

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
   0:	0f 0d 03 [ 	]*prefetch \(%ebx\)
   3:	0f 0d 0c 75 00 10 00 00 [ 	]*prefetchw 0x1000\(,%esi,2\)
   b:	0f 0e [ 	]*femms  
   d:	0f 0f 00 bf [ 	]*pavgusb \(%eax\),%mm0
  11:	0f 0f 48 02 1d [ 	]*pf2id  0x2\(%eax\),%mm1
  16:	0f 0f 90 00 01 00 00 ae [ 	]*pfacc  0x100\(%eax\),%mm2
  1e:	0f 0f 1e 9e [ 	]*pfadd  \(%esi\),%mm3
  22:	0f 0f 66 02 b0 [ 	]*pfcmpeq 0x2\(%esi\),%mm4
  27:	0f 0f ae 90 90 00 00 90 [ 	]*pfcmpge 0x9090\(%esi\),%mm5
  2f:	0f 0f 74 75 00 a0 [ 	]*pfcmpgt 0x0\(%ebp,%esi,2\),%mm6
  35:	0f 0f 7c 75 02 a4 [ 	]*pfmax  0x2\(%ebp,%esi,2\),%mm7
  3b:	0f 0f 84 75 90 90 90 90 94 [ 	]*pfmin  0x90909090\(%ebp,%esi,2\),%mm0
  44:	0f 0f 0d 04 00 00 00 b4 [ 	]*pfmul  0x4,%mm1
  4c:	2e 0f 0f 54 c3 07 96 [ 	]*pfrcp  %cs:0x7\(%ebx,%eax,8\),%mm2
  53:	0f 0f d8 a6 [ 	]*pfrcpit1 %mm0,%mm3
  57:	0f 0f e1 b6 [ 	]*pfrcpit2 %mm1,%mm4
  5b:	0f 0f ea a7 [ 	]*pfrsqit1 %mm2,%mm5
  5f:	0f 0f f3 97 [ 	]*pfrsqrt %mm3,%mm6
  63:	0f 0f fc 9a [ 	]*pfsub  %mm4,%mm7
  67:	0f 0f c5 aa [ 	]*pfsubr %mm5,%mm0
  6b:	0f 0f ce 0d [ 	]*pi2fd  %mm6,%mm1
  6f:	0f 0f d7 b7 [ 	]*pfmulhrw %mm7,%mm2
  73:	2e 0f [ 	]*\(bad\)  
  75:	0f 54 c3 [ 	]*andps  %xmm3,%xmm0
  78:	07 [ 	]*pop    %es
  79:	c3 [ 	]*ret    
  7a:	90 [ 	]*nop    
  7b:	90 [ 	]*nop    
  7c:	90 [ 	]*nop    
  7d:	90 [ 	]*nop    
  7e:	90 [ 	]*nop    
  7f:	90 [ 	]*nop    
