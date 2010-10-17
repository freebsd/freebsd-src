#objdump: -drw
#name: i386 gotpc

.*: +file format .*

Disassembly of section .text:

0+000 <test>:
   0:	05 01 00 00 00 [ 	]*add    \$0x1,%eax	1: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
   5:	81 c3 07 00 00 00 [ 	]*add    \$0x7,%ebx	7: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
   b:	05 01 00 00 00 [ 	]*add    \$0x1,%eax	c: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  10:	81 c3 02 00 00 00 [ 	]*add    \$0x2,%ebx	12: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  16:	8d 98 18 00 00 00 [ 	]*lea    0x18\(%eax\),%ebx	18: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  1c:	8d 83 1e 00 00 00 [ 	]*lea    0x1e\(%ebx\),%eax	1e: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  22:	8d 80 24 00 00 00 [ 	]*lea    0x24\(%eax\),%eax	24: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  28:	8d 9b 2a 00 00 00 [ 	]*lea    0x2a\(%ebx\),%ebx	2a: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  2e:	2d 2f 00 00 00 [ 	]*sub    \$0x2f,%eax	2f: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  33:	81 eb 35 00 00 00 [ 	]*sub    \$0x35,%ebx	35: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  39:	2d 01 00 00 00 [ 	]*sub    \$0x1,%eax	3a: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  3e:	81 eb 02 00 00 00 [ 	]*sub    \$0x2,%ebx	40: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  44:	0d 45 00 00 00 [ 	]*or     \$0x45,%eax	45: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  49:	81 cb 4b 00 00 00 [ 	]*or     \$0x4b,%ebx	4b: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  4f:	0d 01 00 00 00 [ 	]*or     \$0x1,%eax	50: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  54:	81 cb 02 00 00 00 [ 	]*or     \$0x2,%ebx	56: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  5a:	b8 5b 00 00 00 [ 	]*mov    \$0x5b,%eax	5b: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  5f:	bb 60 00 00 00 [ 	]*mov    \$0x60,%ebx	60: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  64:	b8 01 00 00 00 [ 	]*mov    \$0x1,%eax	65: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  69:	bb 01 00 00 00 [ 	]*mov    \$0x1,%ebx	6a: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  6e:	c7 05 00 00 00 00 74 00 00 00 [ 	]*movl   \$0x74,0x0	70: (R_386_)?(dir)?32	foo
[ 	]*74: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  78:	65 c7 05 00 00 00 00 7f 00 00 00 [ 	]*movl   \$0x7f,%gs:0x0	7b: (R_386_)?(dir)?32	foo
[ 	]*7f: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  83:	65 c7 05 00 00 00 00 8a 00 00 00 [ 	]*movl   \$0x8a,%gs:0x0	86: (R_386_)?(dir)?32	foo
[ 	]*8a: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  8e:	c7 05 02 00 00 00 94 00 00 00 [ 	]*movl   \$0x94,0x2	90: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
[ 	]*94: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  98:	a1 99 00 00 00 [ 	]*mov    0x99,%eax	99: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  9d:	8b 1d 9f 00 00 00 [ 	]*mov    0x9f,%ebx	9f: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  a3:	a3 a4 00 00 00 [ 	]*mov    %eax,0xa4	a4: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  a8:	89 1d aa 00 00 00 [ 	]*mov    %ebx,0xaa	aa: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  ae:	65 a3 b0 00 00 00 [ 	]*mov    %eax,%gs:0xb0	b0: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  b4:	65 89 1d b7 00 00 00 [ 	]*mov    %ebx,%gs:0xb7	b7: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  bb:	65 a3 bd 00 00 00 [ 	]*mov    %eax,%gs:0xbd	bd: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  c1:	65 89 1d c4 00 00 00 [ 	]*mov    %ebx,%gs:0xc4	c4: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  c8:	8d 83 00 00 00 00 [ 	]*lea    0x0\(%ebx\),%eax	ca: (R_386_)?GOTOFF	_GLOBAL_OFFSET_TABLE_
  ce:	8d 9b 00 00 00 00 [ 	]*lea    0x0\(%ebx\),%ebx	d0: (R_386_)?GOTOFF	_GLOBAL_OFFSET_TABLE_
  d4:	8b 83 00 00 00 00 [ 	]*mov    0x0\(%ebx\),%eax	d6: (R_386_)?GOTOFF	_GLOBAL_OFFSET_TABLE_
  da:	8b 9b 00 00 00 00 [ 	]*mov    0x0\(%ebx\),%ebx	dc: (R_386_)?GOTOFF	_GLOBAL_OFFSET_TABLE_
  e0:	e0 00 [ 	]*loopne e2 <test\+0xe2>	e0: (R_386_)?GOTPC	_GLOBAL_OFFSET_TABLE_
  e2:	00 00 [ 	]*add    %al,\(%eax\)
  e4:	00 00 [ 	]*add    %al,\(%eax\)	e4: (R_386_)?GOTOFF	_GLOBAL_OFFSET_TABLE_
	...
