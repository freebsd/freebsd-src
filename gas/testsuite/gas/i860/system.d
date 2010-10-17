#as:
#objdump: -dr
#name: i860 system

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	01 00 00 4c 	lock	
   4:	07 00 00 4c 	unlock	
   8:	04 00 00 4c 	intovr	
   c:	00 00 00 44 	trap	%r0,%r0,%r0
  10:	00 f8 ff 47 	trap	%r31,%r31,%r31
  14:	00 08 b2 44 	trap	%r1,%r5,%r18
  18:	00 f8 86 46 	trap	%r31,%r20,%r6
  1c:	00 00 01 30 	ld.c	%fir,%r1
  20:	00 00 1f 30 	ld.c	%fir,%r31
  24:	00 00 25 30 	ld.c	%psr,%r5
  28:	00 00 3e 30 	ld.c	%psr,%r30
  2c:	00 00 4a 30 	ld.c	%dirbase,%r10
  30:	00 00 42 30 	ld.c	%dirbase,%sp
  34:	00 00 75 30 	ld.c	%db,%r21
  38:	00 00 60 30 	ld.c	%db,%r0
  3c:	00 00 9c 30 	ld.c	%fsr,%r28
  40:	00 00 8c 30 	ld.c	%fsr,%r12
  44:	00 00 bf 30 	ld.c	%epsr,%r31
  48:	00 00 a6 30 	ld.c	%epsr,%r6
  4c:	00 00 00 38 	st.c	%r0,%fir
  50:	00 f0 00 38 	st.c	%r30,%fir
  54:	00 38 20 38 	st.c	%r7,%psr
  58:	00 f8 20 38 	st.c	%r31,%psr
  5c:	00 58 40 38 	st.c	%r11,%dirbase
  60:	00 18 40 38 	st.c	%fp,%dirbase
  64:	00 b0 60 38 	st.c	%r22,%db
  68:	00 78 60 38 	st.c	%r15,%db
  6c:	00 e8 80 38 	st.c	%r29,%fsr
  70:	00 68 80 38 	st.c	%r13,%fsr
  74:	00 20 a0 38 	st.c	%r4,%epsr
  78:	00 30 a0 38 	st.c	%r6,%epsr
  7c:	04 00 00 34 	flush	0\(%r0\)
  80:	84 00 20 34 	flush	128\(%r1\)
  84:	04 01 40 34 	flush	256\(%sp\)
  88:	04 02 60 34 	flush	512\(%fp\)
  8c:	04 04 80 34 	flush	1024\(%r4\)
  90:	04 10 a0 34 	flush	4096\(%r5\)
  94:	04 20 c0 34 	flush	8192\(%r6\)
  98:	04 40 e0 34 	flush	16384\(%r7\)
  9c:	04 c0 00 35 	flush	-16384\(%r8\)
  a0:	04 e0 20 35 	flush	-8192\(%r9\)
  a4:	04 f0 40 35 	flush	-4096\(%r10\)
  a8:	04 fc 60 35 	flush	-1024\(%r11\)
  ac:	04 fe 80 35 	flush	-512\(%r12\)
  b0:	0c ff a0 35 	flush	-248\(%r13\)
  b4:	e4 ff c0 35 	flush	-32\(%r14\)
  b8:	f4 ff c0 35 	flush	-16\(%r14\)
  bc:	05 00 00 34 	flush	0\(%r0\)\+\+
  c0:	85 00 20 34 	flush	128\(%r1\)\+\+
  c4:	05 01 40 34 	flush	256\(%sp\)\+\+
  c8:	05 02 60 34 	flush	512\(%fp\)\+\+
  cc:	05 04 80 34 	flush	1024\(%r4\)\+\+
  d0:	05 10 c0 36 	flush	4096\(%r22\)\+\+
  d4:	05 20 e0 36 	flush	8192\(%r23\)\+\+
  d8:	05 40 00 37 	flush	16384\(%r24\)\+\+
  dc:	05 c0 20 37 	flush	-16384\(%r25\)\+\+
  e0:	05 e0 40 37 	flush	-8192\(%r26\)\+\+
  e4:	05 f0 60 37 	flush	-4096\(%r27\)\+\+
  e8:	05 fc 80 37 	flush	-1024\(%r28\)\+\+
  ec:	05 fe a0 37 	flush	-512\(%r29\)\+\+
  f0:	0d ff c0 37 	flush	-248\(%r30\)\+\+
  f4:	25 00 e0 37 	flush	32\(%r31\)\+\+
  f8:	15 00 e0 37 	flush	16\(%r31\)\+\+
