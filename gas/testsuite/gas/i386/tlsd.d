#objdump: -dr
#name: i386 dynamic tls

.*: +file format .*

Disassembly of section .text:

0+000 <fn>:
   0:	55 [ 	]*push   %ebp
   1:	89 e5 [ 	]*mov    %esp,%ebp
   3:	53 [ 	]*push   %ebx
   4:	50 [ 	]*push   %eax
   5:	e8 00 00 00 00 [ 	]*call   a <fn\+0xa>
   a:	5b [ 	]*pop    %ebx
   b:	81 c3 03 00 00 00 [ 	]*add    \$0x3,%ebx
[ 	]+d: R_386_GOTPC	_GLOBAL_OFFSET_TABLE_
  11:	8d 04 1d 00 00 00 00 [ 	]*lea    0x0\(,%ebx,1\),%eax
[ 	]+14: R_386_TLS_GD	foo
  18:	e8 fc ff ff ff [ 	]*call   19 <fn\+0x19>
[ 	]+19: R_386_PLT32	___tls_get_addr
  1d:	8d 83 00 00 00 00 [ 	]*lea    0x0\(%ebx\),%eax
[ 	]+1f: R_386_TLS_LDM	bar
  23:	e8 fc ff ff ff [ 	]*call   24 <fn\+0x24>
[ 	]+24: R_386_PLT32	___tls_get_addr
  28:	83 c7 00 [ 	]*add    \$0x0,%edi
  2b:	8d 90 00 00 00 00 [ 	]*lea    0x0\(%eax\),%edx
[ 	]+2d: R_386_TLS_LDO_32	bar
  31:	83 c6 00 [ 	]*add    \$0x0,%esi
  34:	8d 88 00 00 00 00 [ 	]*lea    0x0\(%eax\),%ecx
[ 	]+36: R_386_TLS_LDO_32	baz
  3a:	8b 5d fc [ 	]*mov    -0x4\(%ebp\),%ebx
  3d:	c9 [ 	]*leave[ 	]*
  3e:	c3 [ 	]*ret[ 	]*
