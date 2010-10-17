#objdump: -dw
#name: i386 prescott

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
   0:	66 0f d0 01 [ 	]*addsubpd \(%ecx\),%xmm0
   4:	66 0f d0 ca [ 	]*addsubpd %xmm2,%xmm1
   8:	f2 0f d0 13 [ 	]*addsubps \(%ebx\),%xmm2
   c:	f2 0f d0 dc [ 	]*addsubps %xmm4,%xmm3
  10:	df 88 90 90 90 90 [ 	]*fisttp 0x90909090\(%eax\)
  16:	db 88 90 90 90 90 [ 	]*fisttpl 0x90909090\(%eax\)
  1c:	dd 88 90 90 90 90 [ 	]*fisttpll 0x90909090\(%eax\)
  22:	dd 88 90 90 90 90 [ 	]*fisttpll 0x90909090\(%eax\)
  28:	dd 88 90 90 90 90 [ 	]*fisttpll 0x90909090\(%eax\)
  2e:	66 0f 7c 65 00 [ 	]*haddpd 0x0\(%ebp\),%xmm4
  33:	66 0f 7c ee [ 	]*haddpd %xmm6,%xmm5
  37:	f2 0f 7c 37 [ 	]*haddps \(%edi\),%xmm6
  3b:	f2 0f 7c f8 [ 	]*haddps %xmm0,%xmm7
  3f:	66 0f 7d c1 [ 	]*hsubpd %xmm1,%xmm0
  43:	66 0f 7d 0a [ 	]*hsubpd \(%edx\),%xmm1
  47:	f2 0f 7d d2 [ 	]*hsubps %xmm2,%xmm2
  4b:	f2 0f 7d 1c 24 [ 	]*hsubps \(%esp\),%xmm3
  50:	f2 0f f0 2e [ 	]*lddqu  \(%esi\),%xmm5
  54:	0f 01 c8 [ 	]*monitor %eax,%ecx,%edx 
  57:	0f 01 c8 [ 	]*monitor %eax,%ecx,%edx 
  5a:	f2 0f 12 f7 [ 	]*movddup %xmm7,%xmm6
  5e:	f2 0f 12 38 [ 	]*movddup \(%eax\),%xmm7
  62:	f3 0f 16 01 [ 	]*movshdup \(%ecx\),%xmm0
  66:	f3 0f 16 ca [ 	]*movshdup %xmm2,%xmm1
  6a:	f3 0f 12 13 [ 	]*movsldup \(%ebx\),%xmm2
  6e:	f3 0f 12 dc [ 	]*movsldup %xmm4,%xmm3
  72:	0f 01 c9 [ 	]*mwait   %eax,%ecx 
  75:	0f 01 c9 [ 	]*mwait   %eax,%ecx 
	...
