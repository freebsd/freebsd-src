#objdump: -d
#name: flow2
.*: +file format .*

Disassembly of section .text:

00000000 <MY_LABEL1-0x2a>:
   0:	50 00       	JUMP  \(P0\);
   2:	51 00       	JUMP  \(P1\);
   4:	52 00       	JUMP  \(P2\);
   6:	53 00       	JUMP  \(P3\);
   8:	54 00       	JUMP  \(P4\);
   a:	55 00       	JUMP  \(P5\);
   c:	56 00       	JUMP  \(SP\);
   e:	57 00       	JUMP  \(FP\);
  10:	80 00       	JUMP  \(PC\+P0\);
  12:	81 00       	JUMP  \(PC\+P1\);
  14:	82 00       	JUMP  \(PC\+P2\);
  16:	83 00       	JUMP  \(PC\+P3\);
  18:	84 00       	JUMP  \(PC\+P4\);
  1a:	85 00       	JUMP  \(PC\+P5\);
  1c:	86 00       	JUMP  \(PC\+SP\);
  1e:	87 00       	JUMP  \(PC\+FP\);
  20:	00 20       	JUMP.S  20 <MY_LABEL1-0xa>;
  22:	69 22       	JUMP.S  4f4.*
  24:	97 2d       	JUMP.S  fffffb52.*
  26:	01 20       	JUMP.S  28 <MY_LABEL1-0x2>;
  28:	ff 2f       	JUMP.S  26 <MY_LABEL1-0x4>;

0000002a <MY_LABEL1>:
  2a:	00 20       	JUMP.S  2a <MY_LABEL1>;
  2c:	69 22       	JUMP.S  4fe.*
  2e:	97 2d       	JUMP.S  fffffb5c.*
  30:	01 20       	JUMP.S  32 <MY_LABEL1\+0x8>;
  32:	ff 2f       	JUMP.S  30 <MY_LABEL1\+0x6>;
  34:	c0 e2 00 00 	JUMP.L  ff800034.*
  38:	3f e2 ff ff 	JUMP.L  800036.*
  3c:	00 e2 00 00 	JUMP.L  3c <MY_LABEL1\+0x12>;
  40:	00 e2 69 02 	JUMP.L  512.*
  44:	ff e2 97 fd 	JUMP.L  fffffb72.*
  48:	00 e2 01 00 	JUMP.L  4a <MY_LABEL1\+0x20>;
  4c:	ff e2 ff ff 	JUMP.L  4a <MY_LABEL1\+0x20>;
  50:	ed 2f       	JUMP.S  2a <MY_LABEL1>;
  52:	d7 2f       	JUMP.S  0 .*
  54:	d6 2f       	JUMP.S  0 .*
  56:	d5 2f       	JUMP.S  0 .*
  58:	04 1b       	IF CC JUMP fffffe60.*
  5a:	5a 18       	IF CC JUMP 10e.*
  5c:	00 18       	IF CC JUMP 5c <MY_LABEL1\+0x32>;
  5e:	04 1f       	IF CC JUMP fffffe66.*\(BP\);
  60:	5a 1c       	IF CC JUMP 114.*\(BP\);
  62:	91 13       	IF ! CC JUMP ffffff84.*;
  64:	90 10       	IF ! CC JUMP 184.*;
  66:	91 17       	IF ! CC JUMP ffffff88.*\(BP\);
  68:	90 14       	IF ! CC JUMP 188.*\(BP\);
  6a:	e0 1b       	IF CC JUMP 2a <MY_LABEL1>;
  6c:	ca 1b       	IF CC JUMP 0 <MY_LABEL1-0x2a>;
  6e:	de 1f       	IF CC JUMP 2a <MY_LABEL1>\(BP\);
  70:	c8 1f       	IF CC JUMP 0 <MY_LABEL1-0x2a>\(BP\);
  72:	dc 13       	IF ! CC JUMP 2a <MY_LABEL1>;
  74:	c6 13       	IF ! CC JUMP 0 <MY_LABEL1-0x2a>;
  76:	da 17       	IF ! CC JUMP 2a <MY_LABEL1>\(BP\);
  78:	c4 17       	IF ! CC JUMP 0 <MY_LABEL1-0x2a>\(BP\);
  7a:	60 00       	CALL  \(P0\);
  7c:	61 00       	CALL  \(P1\);
  7e:	62 00       	CALL  \(P2\);
  80:	63 00       	CALL  \(P3\);
  82:	64 00       	CALL  \(P4\);
  84:	65 00       	CALL  \(P5\);
  86:	70 00       	CALL  \(PC\+P0\);
  88:	71 00       	CALL  \(PC\+P1\);
  8a:	72 00       	CALL  \(PC\+P2\);
  8c:	73 00       	CALL  \(PC\+P3\);
  8e:	74 00       	CALL  \(PC\+P4\);
  90:	75 00       	CALL  \(PC\+P5\);
  92:	09 e3 2b 1a 	CALL  1234e8.*;
  96:	ff e3 97 fd 	CALL  fffffbc4.*;
  9a:	ff e3 c8 ff 	CALL  2a <MY_LABEL1>;
  9e:	ff e3 b1 ff 	CALL  0 <MY_LABEL1-0x2a>;
  a2:	10 00       	RTS;
  a4:	11 00       	RTI;
  a6:	12 00       	RTX;
  a8:	13 00       	RTN;
  aa:	14 00       	RTE;
  ac:	82 e0 02 00 	LSETUP\(b0 <MY_LABEL1\+0x86>,b0 <MY_LABEL1\+0x86>\)LC0;
  b0:	84 e0 06 00 	LSETUP\(b8 <beg_poll_bit>,bc <end_poll_bit>\)LC0;
  b4:	00 00       	NOP;
	...

000000b8 <beg_poll_bit>:
  b8:	80 e1 01 00 	R0=1 <MY_LABEL1-0x29>\(Z\);

000000bc <end_poll_bit>:
  bc:	81 e1 02 00 	R1=2 <MY_LABEL1-0x28>\(Z\);
  c0:	92 e0 03 00 	LSETUP\(c4 <end_poll_bit\+0x8>,c6 <end_poll_bit\+0xa>\)LC1;
  c4:	93 e0 05 00 	LSETUP\(ca <FIR_filter>,ce <bottom_of_FIR_filter>\)LC1;
	...

000000ca <FIR_filter>:
  ca:	80 e1 01 00 	R0=1 <MY_LABEL1-0x29>\(Z\);

000000ce <bottom_of_FIR_filter>:
  ce:	81 e1 02 00 	R1=2 <MY_LABEL1-0x28>\(Z\);
  d2:	a2 e0 04 10 	LSETUP\(d6 <bottom_of_FIR_filter\+0x8>,da <bottom_of_FIR_filter\+0xc>\)LC0=P1;
  d6:	e2 e0 04 10 	LSETUP\(da <bottom_of_FIR_filter\+0xc>,de <DoItSome__BEGIN>\)LC0=P1>>1;
  da:	82 e0 03 00 	LSETUP\(de <DoItSome__BEGIN>,e0 <DoItSome__END>\)LC0;

000000de <DoItSome__BEGIN>:
  de:	08 60       	R0=0x1\(x\);

000000e0 <DoItSome__END>:
  e0:	11 60       	R1=0x2\(x\);
  e2:	90 e0 00 00 	LSETUP\(e2 <DoItSome__END\+0x2>,e2 <DoItSome__END\+0x2>\)LC1;
	...
