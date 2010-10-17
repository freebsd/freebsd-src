#as: -Asparclet
#objdump: -dr
#name: sparclet coprocessor registers

.*: +file format .*

Disassembly of section .text:

0+ <start>:
   0:	81 b0 40 c0 	cwrcxt  %g1, %ccsr
   4:	83 b0 40 c0 	cwrcxt  %g1, %ccfr
   8:	85 b0 40 c0 	cwrcxt  %g1, %cccrcr
   c:	87 b0 40 c0 	cwrcxt  %g1, %ccpr
  10:	89 b0 40 c0 	cwrcxt  %g1, %ccsr2
  14:	8b b0 40 c0 	cwrcxt  %g1, %cccrr
  18:	8d b0 40 c0 	cwrcxt  %g1, %ccrstr
  1c:	83 b0 01 00 	crdcxt  %ccsr, %g1
  20:	83 b0 41 00 	crdcxt  %ccfr, %g1
  24:	83 b0 81 00 	crdcxt  %cccrcr, %g1
  28:	83 b0 c1 00 	crdcxt  %ccpr, %g1
  2c:	83 b1 01 00 	crdcxt  %ccsr2, %g1
  30:	83 b1 41 00 	crdcxt  %cccrr, %g1
  34:	83 b1 81 00 	crdcxt  %ccrstr, %g1
