#objdump: -dw
#name: x86-64 prescott

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
   0:	66 0f d0 01 [ 	]*addsubpd \(%rcx\),%xmm0
   4:	66 0f d0 ca [ 	]*addsubpd %xmm2,%xmm1
   8:	f2 0f d0 13 [ 	]*addsubps \(%rbx\),%xmm2
   c:	f2 0f d0 dc [ 	]*addsubps %xmm4,%xmm3
  10:	df 88 90 90 90 90 [ 	]*fisttp 0xffffffff90909090\(%rax\)
  16:	db 88 90 90 90 90 [ 	]*fisttpl 0xffffffff90909090\(%rax\)
  1c:	dd 88 90 90 90 90 [ 	]*fisttpll 0xffffffff90909090\(%rax\)
  22:	66 0f 7c 65 00 [ 	]*haddpd 0x0\(%rbp\),%xmm4
  27:	66 0f 7c ee [ 	]*haddpd %xmm6,%xmm5
  2b:	f2 0f 7c 37 [ 	]*haddps \(%rdi\),%xmm6
  2f:	f2 0f 7c f8 [ 	]*haddps %xmm0,%xmm7
  33:	66 0f 7d c1 [ 	]*hsubpd %xmm1,%xmm0
  37:	66 0f 7d 0a [ 	]*hsubpd \(%rdx\),%xmm1
  3b:	f2 0f 7d d2 [ 	]*hsubps %xmm2,%xmm2
  3f:	f2 0f 7d 1c 24 [ 	]*hsubps \(%rsp\),%xmm3
  44:	f2 0f f0 2e [ 	]*lddqu  \(%rsi\),%xmm5
  48:	0f 01 c8 [ 	]*monitor %rax,%rcx,%rdx
  4b:	0f 01 c8 [ 	]*monitor %rax,%rcx,%rdx
  4e:	f2 0f 12 f7 [ 	]*movddup %xmm7,%xmm6
  52:	f2 0f 12 38 [ 	]*movddup \(%rax\),%xmm7
  56:	f3 0f 16 01 [ 	]*movshdup \(%rcx\),%xmm0
  5a:	f3 0f 16 ca [ 	]*movshdup %xmm2,%xmm1
  5e:	f3 0f 12 13 [ 	]*movsldup \(%rbx\),%xmm2
  62:	f3 0f 12 dc [ 	]*movsldup %xmm4,%xmm3
  66:	0f 01 c9 [ 	]*mwait  %rax,%rcx
  69:	0f 01 c9 [ 	]*mwait  %rax,%rcx
  6c:	67 0f 01 c8 [ 	]*monitor %eax,%rcx,%rdx
  70:	67 0f 01 c8 [ 	]*monitor %eax,%rcx,%rdx
	...
