#as:
#objdump: -dr
#name: i860 regress01

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	f2 ff 50 2c 	fst.l	%f16,-16\(%sp\)
   4:	f2 ff 54 2c 	fst.l	%f20,-16\(%sp\)
   8:	f2 ff 58 2c 	fst.l	%f24,-16\(%sp\)
   c:	f3 ff 50 2c 	fst.l	%f16,-16\(%sp\)\+\+
  10:	f3 ff 54 2c 	fst.l	%f20,-16\(%sp\)\+\+
  14:	f3 ff 58 2c 	fst.l	%f24,-16\(%sp\)\+\+
  18:	f4 ff 50 2c 	fst.q	%f16,-16\(%sp\)
  1c:	f4 ff 54 2c 	fst.q	%f20,-16\(%sp\)
  20:	f4 ff 58 2c 	fst.q	%f24,-16\(%sp\)
  24:	f5 ff 50 2c 	fst.q	%f16,-16\(%sp\)\+\+
  28:	f5 ff 54 2c 	fst.q	%f20,-16\(%sp\)\+\+
  2c:	f5 ff 58 2c 	fst.q	%f24,-16\(%sp\)\+\+
