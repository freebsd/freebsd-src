#name: pdp11 opcode
#objdump: -drw

dump.o: +file format .*

Disassembly of section .text:

00000000 <foo>:
	...

00000002 <bar>:
	...
	2: 16	\*ABS\*

00000004 <start>:
	...

00000006 <start2>:
   6:	0001 [	 ]*wait
   8:	0002 [	 ]*rti
   a:	0003 [	 ]*bpt
   c:	0004 [	 ]*iot
   e:	0005 [	 ]*reset
  10:	0006 [	 ]*rtt
  12:	0007 [	 ]*mfpt
  14:	0051 [	 ]*jmp	\(r1\)\+
  16:	0082 [	 ]*rts	r2
  18:	009b [	 ]*spl	3
  1a:	00a0 [	 ]*nop
  1c:	00a1 [	 ]*clc
  1e:	00a2 [	 ]*clv
  20:	00a4 [	 ]*clz
  22:	00a8 [	 ]*cln
  24:	00af [	 ]*ccc
  26:	00a1 [	 ]*clc
  28:	00b2 [	 ]*sev
  2a:	00b4 [	 ]*sez
  2c:	00b8 [	 ]*sen
  2e:	00bf [	 ]*scc
  30:	00c7 [	 ]*swab	pc
  32:	01ff [	 ]*br	32 <start2\+0x2c>
  34:	02fe [	 ]*bne	32 <start2\+0x2c>
  36:	03fd [	 ]*beq	32 <start2\+0x2c>
  38:	04fc [	 ]*bge	32 <start2\+0x2c>
  3a:	05fb [	 ]*blt	32 <start2\+0x2c>
  3c:	06fa [	 ]*bgt	32 <start2\+0x2c>
  3e:	07f9 [	 ]*ble	32 <start2\+0x2c>
  40:	09de [	 ]*jsr	pc, \*\(sp\)\+
  42:	0a26 [	 ]*clr	-\(sp\)
  44:	0a40 [	 ]*com	r0
  46:	0a81 [	 ]*inc	r1
  48:	0ac2 [	 ]*dec	r2
  4a:	0b03 [	 ]*neg	r3
  4c:	0b44 [	 ]*adc	r4
  4e:	0b85 [	 ]*sbc	r5
  50:	0bd6 [	 ]*tst	\(sp\)\+
  52:	0c05 [	 ]*ror	r5
  54:	0c44 [	 ]*rol	r4
  56:	0cbc 000a [	 ]*asr	\*12\(r4\)
  5a:	0cf5 0004 [	 ]*asl	4\(r5\)
  5e:	0d02 [	 ]*mark	2
  60:	0d46 [	 ]*mfpi	sp
  62:	0d9f 0192 [	 ]*mtpi	\*\$622
  66:	0dc3 [	 ]*sxt	r3
  68:	0e34 0002 [	 ]*csm	2\(r4\)
  6c:	0e4b [	 ]*tstset	\(r3\)
  6e:	0eb4 0002 [	 ]*wrtlck	2\(r4\)
  72:	1001 [	 ]*mov	r0, r1
  74:	220c [	 ]*cmp	\(r0\), \(r4\)
  76:	3423 [	 ]*bit	\(r0\)\+, -\(r3\)
  78:	4dff ff84 ff84 [	 ]*bic	\$0 <foo>, \*\$2 <bar>
  7e:	566d [	 ]*bis	\*\(r1\)\+, \*-\(r5\)
  80:	6cfb 0004 0006 [	 ]*add	4\(r3\), \*6\(r3\)
  86:	7097 000a [	 ]*mul	\$12, r2
  8a:	7337 ffa4 [	 ]*div	\$32 <start2\+0x2c>, r4
  8e:	7517 0003 [	 ]*ash	\$3, r4
  92:	7697 0007 [	 ]*ashc	\$7, r2
  96:	78f6 000a [	 ]*xor	r3, 12\(sp\)
  9a:	7a02 [	 ]*fadd	r2
  9c:	7a09 [	 ]*fsub	r1
  9e:	7a14 [	 ]*fmul	r4
  a0:	7a18 [	 ]*fdiv	r0
  a2:	7c11 [	 ]*l2dr	r1
  a4:	7c18 [	 ]*movc
  a6:	7c19 [	 ]*movrc
  a8:	7c1a [	 ]*movtc
  aa:	7c20 [	 ]*locc
  ac:	7c21 [	 ]*skpc
  ae:	7c22 [	 ]*scanc
  b0:	7c23 [	 ]*spanc
  b2:	7c24 [	 ]*cmpc
  b4:	7c25 [	 ]*matc
  b6:	7c28 [	 ]*addn
  b8:	7c29 [	 ]*subn
  ba:	7c2a [	 ]*cmpn
  bc:	7c2b [	 ]*cvtnl
  be:	7c2c [	 ]*cvtpn
  c0:	7c2d [	 ]*cvtnp
  c2:	7c2e [	 ]*ashn
  c4:	7c2f [	 ]*cvtln
  c6:	7c35 [	 ]*l3dr	r5
  c8:	7c38 [	 ]*addp
  ca:	7c39 [	 ]*subp
  cc:	7c3a [	 ]*cmpp
  ce:	7c3b [	 ]*cvtpl
  d0:	7c3c [	 ]*mulp
  d2:	7c3d [	 ]*divp
  d4:	7c3e [	 ]*ashp
  d6:	7c3f [	 ]*cvtlp
  d8:	7c58 [	 ]*movci
  da:	7c59 [	 ]*movrci
  dc:	7c5a [	 ]*movtci
  de:	7c60 [	 ]*locci
  e0:	7c61 [	 ]*skpci
  e2:	7c62 [	 ]*scanci
  e4:	7c63 [	 ]*spanci
  e6:	7c64 [	 ]*cmpci
  e8:	7c65 [	 ]*matci
  ea:	7c68 [	 ]*addni
  ec:	7c69 [	 ]*subni
  ee:	7c6a [	 ]*cmpni
  f0:	7c6b [	 ]*cvtnli
  f2:	7c6c [	 ]*cvtpni
  f4:	7c6d [	 ]*cvtnpi
  f6:	7c6e [	 ]*ashni
  f8:	7c6f [	 ]*cvtlni
  fa:	7c78 [	 ]*addpi
  fc:	7c79 [	 ]*subpi
  fe:	7c7a [	 ]*cmppi
 100:	7c7b [	 ]*cvtpli
 102:	7c7c [	 ]*mulpi
 104:	7c7d [	 ]*divpi
 106:	7c7e [	 ]*ashpi
 108:	7c7f [	 ]*cvtlpi
 10a:	7d80 [	 ]*med
 10c:	7dea [	 ]*xfc	52
 10e:	7e3e [	 ]*sob	r0, 10c <start2\+0x106>
 110:	80fd [	 ]*bpl	10c <start2\+0x106>
 112:	81fc [	 ]*bmi	10c <start2\+0x106>
 114:	82fb [	 ]*bhi	10c <start2\+0x106>
 116:	83fa [	 ]*blos	10c <start2\+0x106>
 118:	84f9 [	 ]*bvc	10c <start2\+0x106>
 11a:	85f8 [	 ]*bvs	10c <start2\+0x106>
 11c:	86f7 [	 ]*bcc	10c <start2\+0x106>
 11e:	87f6 [	 ]*bcs	10c <start2\+0x106>
 120:	8845 [	 ]*emt	105
 122:	892a [	 ]*sys	52
 124:	8a0b [	 ]*clrb	\(r3\)
 126:	8a6d [	 ]*comb	\*-\(r5\)
 128:	8a9e [	 ]*incb	\*\(sp\)\+
 12a:	8ac3 [	 ]*decb	r3
 12c:	8b37 fed0 [	 ]*negb	\$0 <foo>
 130:	8b7f fece [	 ]*adcb	\*\$2 <bar>
 134:	8ba2 [	 ]*sbcb	-\(r2\)
 136:	8bd4 [	 ]*tstb	\(r4\)\+
 138:	8c01 [	 ]*rorb	r1
 13a:	8c42 [	 ]*rolb	r2
 13c:	8c83 [	 ]*asrb	r3
 13e:	8cc4 [	 ]*aslb	r4
 140:	8d17 00e0 [	 ]*mtps	\$340
 144:	8d46 [	 ]*mfpd	sp
 146:	8d88 [	 ]*mtpd	\(r0\)
 148:	8de6 [	 ]*mfps	-\(sp\)
 14a:	95f7 0011 feb0 [	 ]*movb	\$21, \$0 <foo>
 150:	a04a [	 ]*cmpb	r1, \(r2\)
 152:	b5c5 004f [	 ]*bitb	\$117, r5
 156:	c5f7 0001 fea6 [	 ]*bicb	\$1, \$2 <bar>
 15c:	d5ff 0002 fea0 [	 ]*bisb	\$2, \*\$2 <bar>
 162:	e005 [	 ]*sub	r0, r5
 164:	f000 [	 ]*cfcc
 166:	f001 [	 ]*setf
 168:	f002 [	 ]*seti
 16a:	f003 [	 ]*ldub
 16c:	f009 [	 ]*setd
 16e:	f00a [	 ]*setl
 170:	f057 0001 [	 ]*ldfps	\$1
 174:	f0a6 [	 ]*stfps	-\(sp\)
 176:	f0ca [	 ]*stst	\(r2\)
 178:	f103 [	 ]*clrf	fr3
 17a:	f141 [	 ]*tstf	fr1
 17c:	f182 [	 ]*absf	fr2
 17e:	f1c0 [	 ]*negf	fr0
 180:	f257 3f80 [	 ]*mulf	\$37600, fr1
 184:	f305 [	 ]*modf	fr5, fr0
 186:	f4b7 fe76 [	 ]*addf	\$0 <foo>, fr2
 18a:	f57f fe74 [	 ]*ldf	\*\$2 <bar>, fr1
 18e:	f6c4 [	 ]*subf	fr4, fr3
 190:	f785 [	 ]*cmpf	fr5, fr2
 192:	f866 [	 ]*stf	fr1, -\(sp\)
 194:	f917 42a0 [	 ]*divf	\$41240, fr0
 198:	fa85 [	 ]*stexp	fr2, r5
 19a:	fbc0 [	 ]*stcfi	fr3, r0
 19c:	fcc5 [	 ]*stcff	fr3, fr5
 19e:	fd80 [	 ]*ldexp	r0, fr2
 1a0:	fec2 [	 ]*ldcif	r2, fr3
 1a2:	ff85 [	 ]*ldcff	fr5, fr2
 1a4:	7c11 [	 ]*l2dr	r1
 1a6:	7c34 [	 ]*l3dr	r4
 1a8:	86fe [	 ]*bcc	1a6 <start2\+0x1a0>
 1aa:	87fd [	 ]*bcs	1a6 <start2\+0x1a0>
 1ac:	8963 [	 ]*sys	143
 1ae:	f103 [	 ]*clrf	fr3
 1b0:	f142 [	 ]*tstf	fr2
 1b2:	f181 [	 ]*absf	fr1
 1b4:	f1c0 [	 ]*negf	fr0
 1b6:	f285 [	 ]*mulf	fr5, fr2
 1b8:	f304 [	 ]*modf	fr4, fr0
 1ba:	f4c4 [	 ]*addf	fr4, fr3
 1bc:	f537 fe42 [	 ]*ldf	\$2 <bar>, fr0
 1c0:	f6b7 fe3c [	 ]*subf	\$0 <foo>, fr2
 1c4:	f785 [	 ]*cmpf	fr5, fr2
 1c6:	f84a [	 ]*stf	fr1, \(r2\)
 1c8:	f9d6 [	 ]*divf	\(sp\)\+, fr3
 1ca:	fb85 [	 ]*stcfi	fr2, r5
 1cc:	fbc0 [	 ]*stcfi	fr3, r0
 1ce:	fb84 [	 ]*stcfi	fr2, r4
 1d0:	fc85 [	 ]*stcff	fr2, fr5
 1d2:	fc44 [	 ]*stcff	fr1, fr4
 1d4:	fe40 [	 ]*ldcif	r0, fr1
 1d6:	fe84 [	 ]*ldcif	r4, fr2
 1d8:	fed7 3977 [	 ]*ldcif	\$34567, fr3
 1dc:	ff85 [	 ]*ldcff	fr5, fr2
 1de:	ff04 [	 ]*ldcff	fr4, fr0
