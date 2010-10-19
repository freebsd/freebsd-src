#as: --underscore --em=criself --march=v32
#objdump: -dr

# Test that lapc shrinks to lapcq and that offsets are emitted correctly.

.*:     file format elf32-us-cris

Disassembly of section \.text:

0+ <a>:
   0:	70a9                	lapcq 0 <a>,r10
   2:	71b9                	lapcq 4 <x>,r11

0+4 <x>:
   4:	72c9                	lapcq 8 <xx>,r12
   6:	b005                	nop 

0+8 <xx>:
   8:	73d9                	lapcq e <xxx>,r13
   a:	b005                	nop 
   c:	b005                	nop 

0+e <xxx>:
   e:	b005                	nop 

0+10 <a00>:
  10:	b005                	nop 
  12:	7f9d feff ffff      	lapc 10 <a00>,r9

0+18 <a0>:
  18:	7089                	lapcq 18 <a0>,r8
  1a:	7179                	lapcq 1c <x0>,r7

0+1c <x0>:
  1c:	7269                	lapcq 20 <xx0>,r6
  1e:	b005                	nop 

0+20 <xx0>:
  20:	b005                	nop 

0+22 <a11>:
  22:	b005                	nop 
  24:	7fad feff ffff      	lapc 22 <a11>,r10

0+2a <a1>:
  2a:	7fad 0000 0000      	lapc 2a <a1>,r10
  30:	7fbd 0600 0000      	lapc 36 <x1>,r11

0+36 <x1>:
  36:	7fcd 0800 0000      	lapc 3e <xx1>,r12
  3c:	b005                	nop 

0+3e <xx1>:
  3e:	7fdd 0a00 0000      	lapc 48 <xxx1>,r13
  44:	b005                	nop 
  46:	b005                	nop 

0+48 <xxx1>:
  48:	b005                	nop 
  4a:	7f39                	lapcq 68 <y>,r3
	\.\.\.
