#as:
#objdump:  -dr
#name:  lsh_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	71 40       	ashub	\$7:s,r1
   2:	91 09       	lshb	\$-7:s,r1
   4:	41 40       	ashub	\$4:s,r1
   6:	c1 09       	lshb	\$-4:s,r1
   8:	81 09       	lshb	\$-8:s,r1
   a:	31 40       	ashub	\$3:s,r1
   c:	d1 09       	lshb	\$-3:s,r1
   e:	21 44       	lshb	r2,r1
  10:	34 44       	lshb	r3,r4
  12:	56 44       	lshb	r5,r6
  14:	8a 44       	lshb	r8,r10
  16:	71 42       	ashuw	\$7:s,r1
  18:	91 49       	lshw	\$-7:s,r1
  1a:	41 42       	ashuw	\$4:s,r1
  1c:	c1 49       	lshw	\$-4:s,r1
  1e:	81 42       	ashuw	\$8:s,r1
  20:	81 49       	lshw	\$-8:s,r1
  22:	31 42       	ashuw	\$3:s,r1
  24:	d1 49       	lshw	\$-3:s,r1
  26:	21 46       	lshw	r2,r1
  28:	34 46       	lshw	r3,r4
  2a:	56 46       	lshw	r5,r6
  2c:	8a 46       	lshw	r8,r10
  2e:	72 4c       	ashud	\$7:s,\(r3,r2\)
  30:	92 4b       	lshd	\$-7:s,\(r3,r2\)
  32:	82 4c       	ashud	\$8:s,\(r3,r2\)
  34:	82 4b       	lshd	\$-8:s,\(r3,r2\)
  36:	42 4c       	ashud	\$4:s,\(r3,r2\)
  38:	c2 4b       	lshd	\$-4:s,\(r3,r2\)
  3a:	c2 4c       	ashud	\$12:s,\(r3,r2\)
  3c:	42 4b       	lshd	\$-12:s,\(r3,r2\)
  3e:	31 4c       	ashud	\$3:s,\(r2,r1\)
  40:	d1 4b       	lshd	\$-3:s,\(r2,r1\)
  42:	41 47       	lshd	r4,\(r2,r1\)
  44:	51 47       	lshd	r5,\(r2,r1\)
  46:	61 47       	lshd	r6,\(r2,r1\)
  48:	81 47       	lshd	r8,\(r2,r1\)
  4a:	11 47       	lshd	r1,\(r2,r1\)
