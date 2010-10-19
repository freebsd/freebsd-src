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
  22:	66 0f 7c 65 00 [ 	]*haddpd 0x0\(%ebp\),%xmm4
  27:	66 0f 7c ee [ 	]*haddpd %xmm6,%xmm5
  2b:	f2 0f 7c 37 [ 	]*haddps \(%edi\),%xmm6
  2f:	f2 0f 7c f8 [ 	]*haddps %xmm0,%xmm7
  33:	66 0f 7d c1 [ 	]*hsubpd %xmm1,%xmm0
  37:	66 0f 7d 0a [ 	]*hsubpd \(%edx\),%xmm1
  3b:	f2 0f 7d d2 [ 	]*hsubps %xmm2,%xmm2
  3f:	f2 0f 7d 1c 24 [ 	]*hsubps \(%esp\),%xmm3
  44:	f2 0f f0 2e [ 	]*lddqu  \(%esi\),%xmm5
  48:	0f 01 c8 [ 	]*monitor %eax,%ecx,%edx
  4b:	0f 01 c8 [ 	]*monitor %eax,%ecx,%edx
  4e:	f2 0f 12 f7 [ 	]*movddup %xmm7,%xmm6
  52:	f2 0f 12 38 [ 	]*movddup \(%eax\),%xmm7
  56:	f3 0f 16 01 [ 	]*movshdup \(%ecx\),%xmm0
  5a:	f3 0f 16 ca [ 	]*movshdup %xmm2,%xmm1
  5e:	f3 0f 12 13 [ 	]*movsldup \(%ebx\),%xmm2
  62:	f3 0f 12 dc [ 	]*movsldup %xmm4,%xmm3
  66:	0f 01 c9 [ 	]*mwait  %eax,%ecx
  69:	0f 01 c9 [ 	]*mwait  %eax,%ecx
  6c:	67 0f 01 c8 [ 	]*monitor %ax,%ecx,%edx
  70:	67 0f 01 c8 [ 	]*monitor %ax,%ecx,%edx
	...
