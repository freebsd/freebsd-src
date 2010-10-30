#as:
#objdump:  -dr
#name:  ash_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	71 40       	ashub	\$7:s,r1
   2:	91 40       	ashub	\$-7:s,r1
   4:	41 40       	ashub	\$4:s,r1
   6:	c1 40       	ashub	\$-4:s,r1
   8:	81 40       	ashub	\$-8:s,r1
   a:	31 40       	ashub	\$3:s,r1
   c:	d1 40       	ashub	\$-3:s,r1
   e:	21 41       	ashub	r2,r1
  10:	34 41       	ashub	r3,r4
  12:	56 41       	ashub	r5,r6
  14:	8a 41       	ashub	r8,r10
  16:	71 42       	ashuw	\$7:s,r1
  18:	91 43       	ashuw	\$-7:s,r1
  1a:	41 42       	ashuw	\$4:s,r1
  1c:	c1 43       	ashuw	\$-4:s,r1
  1e:	81 42       	ashuw	\$8:s,r1
  20:	81 43       	ashuw	\$-8:s,r1
  22:	31 42       	ashuw	\$3:s,r1
  24:	d1 43       	ashuw	\$-3:s,r1
  26:	21 45       	ashuw	r2,r1
  28:	34 45       	ashuw	r3,r4
  2a:	56 45       	ashuw	r5,r6
  2c:	8a 45       	ashuw	r8,r10
  2e:	72 4c       	ashud	\$7:s,\(r3,r2\)
  30:	92 4f       	ashud	\$-7:s,\(r3,r2\)
  32:	82 4c       	ashud	\$8:s,\(r3,r2\)
  34:	82 4f       	ashud	\$-8:s,\(r3,r2\)
  36:	42 4c       	ashud	\$4:s,\(r3,r2\)
  38:	c2 4f       	ashud	\$-4:s,\(r3,r2\)
  3a:	c2 4c       	ashud	\$12:s,\(r3,r2\)
  3c:	42 4f       	ashud	\$-12:s,\(r3,r2\)
  3e:	31 4c       	ashud	\$3:s,\(r2,r1\)
  40:	d1 4f       	ashud	\$-3:s,\(r2,r1\)
  42:	41 48       	ashud	r4,\(r2,r1\)
  44:	51 48       	ashud	r5,\(r2,r1\)
  46:	61 48       	ashud	r6,\(r2,r1\)
  48:	81 48       	ashud	r8,\(r2,r1\)
  4a:	11 48       	ashud	r1,\(r2,r1\)
