#objdump: -dr
#name: i386 non-pic tls

.*: +file format .*

Disassembly of section .text:

0+000 <fn>:
   0:	8b 15 00 00 00 00 [ 	]*mov    0x0,%edx
[ 	]+2: R_386_32	.text
   6:	81 c2 08 00 00 00 [ 	]*add    \$0x8,%edx
[ 	]+8: R_386_GOTPC	_GLOBAL_OFFSET_TABLE_
   c:	65 a1 00 00 00 00 [ 	]*mov    %gs:0x0,%eax
  12:	2b 82 00 00 00 00 [ 	]*sub    0x0\(%edx\),%eax
[ 	]+14: R_386_TLS_IE_32	foo
  18:	65 a1 00 00 00 00 [ 	]*mov    %gs:0x0,%eax
  1e:	2d 00 00 00 00 [ 	]*sub    \$0x0,%eax
[ 	]+1f: R_386_TLS_LE_32	bar
  23:	65 8b 0d 00 00 00 00 [ 	]*mov    %gs:0x0,%ecx
  2a:	90 [ 	]*nop[ 	]*
  2b:	81 e9 00 00 00 00 [ 	]*sub    \$0x0,%ecx
[ 	]+2d: R_386_TLS_LE_32	baz
  31:	65 8b 0d 00 00 00 00 [ 	]*mov    %gs:0x0,%ecx
  38:	90 [ 	]*nop    
  39:	90 [ 	]*nop    
  3a:	8d 81 00 00 00 00 [ 	]*lea    0x0\(%ecx\),%eax
[ 	]+3c: R_386_TLS_LE	var
  40:	90 [ 	]*nop    
  41:	8d 91 00 00 00 00    	lea    0x0\(%ecx\),%edx
[ 	]+43: R_386_TLS_LE	var2
  47:	a1 00 00 00 00 [ 	]*mov    0x0,%eax
[ 	]+48: R_386_TLS_IE	foo
  4c:	65 8b 00 [ 	]*mov    %gs:\(%eax\),%eax
  4f:	65 a1 00 00 00 00 [ 	]*mov    %gs:0x0,%eax
  55:	03 05 00 00 00 00 [ 	]*add    0x0,%eax
			57: R_386_TLS_IE	foo
  5b:	c3 [ 	]*ret[ 	]*
