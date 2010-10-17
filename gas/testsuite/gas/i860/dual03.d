#as: -mintel-syntax
#objdump: -d
#name: i860 dual03

.*: +file format .*

Disassembly of section \.text:

00000000 <L1-0x20>:
   0:	00 00 14 22 	fld.d	%r0\(%r16\),%f20
   4:	fe ff 15 94 	adds	-2,%r0,%r21
   8:	30 06 00 48 	d.pfadd.ss	%f0,%f0,%f0
   c:	fa ff 31 96 	adds	-6,%r17,%r17
  10:	30 06 00 48 	d.pfadd.ss	%f0,%f0,%f0
  14:	02 a8 20 b6 	bla	%r21,%r17,0x00000020	// 20 <L1>
  18:	30 06 00 48 	d.pfadd.ss	%f0,%f0,%f0
  1c:	09 00 16 26 	fld.d	8\(%r16\)\+\+,%f22

00000020 <L1>:
  20:	30 a6 de 4b 	d.pfadd.ss	%f20,%f30,%f30
  24:	06 a8 20 b6 	bla	%r21,%r17,0x00000040	// 40 <L2>
  28:	30 ae ff 4b 	d.pfadd.ss	%f21,%f31,%f31
  2c:	09 00 14 26 	fld.d	8\(%r16\)\+\+,%f20
  30:	30 a6 de 4b 	d.pfadd.ss	%f20,%f30,%f30
  34:	0a 00 00 68 	br	0x00000060	// 60 <S>
  38:	30 ae ff 4b 	d.pfadd.ss	%f21,%f31,%f31
  3c:	00 00 00 a0 	shl	%r0,%r0,%r0

00000040 <L2>:
  40:	30 b6 de 4b 	d.pfadd.ss	%f22,%f30,%f30
  44:	f6 af 3f b6 	bla	%r21,%r17,0x00000020	// 20 <L1>
  48:	30 be ff 4b 	d.pfadd.ss	%f23,%f31,%f31
  4c:	09 00 16 26 	fld.d	8\(%r16\)\+\+,%f22
  50:	30 a6 de 4b 	d.pfadd.ss	%f20,%f30,%f30
  54:	00 00 00 a0 	shl	%r0,%r0,%r0
  58:	30 ae ff 4b 	d.pfadd.ss	%f21,%f31,%f31
  5c:	00 00 00 a0 	shl	%r0,%r0,%r0

00000060 <S>:
  60:	30 b4 de 4b 	pfadd.ss	%f22,%f30,%f30
  64:	fc ff 15 94 	adds	-4,%r0,%r21
  68:	30 bc ff 4b 	pfadd.ss	%f23,%f31,%f31
  6c:	02 a8 20 5a 	bte	%r21,%r17,0x00000078	// 78 <DONE>
  70:	0b 00 14 26 	fld.l	8\(%r16\)\+\+,%f20
  74:	30 a4 de 4b 	pfadd.ss	%f20,%f30,%f30

00000078 <DONE>:
  78:	30 04 1e 48 	pfadd.ss	%f0,%f0,%f30
  7c:	30 f4 ff 4b 	pfadd.ss	%f30,%f31,%f31
  80:	30 04 1e 48 	pfadd.ss	%f0,%f0,%f30
  84:	30 04 00 48 	pfadd.ss	%f0,%f0,%f0
  88:	30 04 1f 48 	pfadd.ss	%f0,%f0,%f31
  8c:	30 f0 f0 4b 	fadd.ss	%f30,%f31,%f16
