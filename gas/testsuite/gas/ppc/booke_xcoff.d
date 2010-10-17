#as: -mppc32 -mbooke32
#objdump: -mpowerpc -dr -Mbooke32
#name: xcoff BookE tests

.*:     file format aixcoff-rs6000

Disassembly of section .text:

0000000000000000 <.text>:
   0:	7c 22 3f 64 	tlbre   r1,r2,7
   4:	7c be 1f a4 	tlbwe   r5,r30,3
   8:	7c a8 48 2c 	icbt    5,r8,r9
   c:	7c a6 02 26 	mfapidi r5,r6
  10:	7c 07 46 24 	tlbivax r7,r8
  14:	7c 09 56 26 	tlbivaxe r9,r10
  18:	7c 0b 67 24 	tlbsx   r11,r12
  1c:	7c 0d 77 26 	tlbsxe  r13,r14
  20:	7e 80 04 40 	mcrxr64 cr5
  24:	4c 00 00 66 	rfci
  28:	7c 60 01 06 	wrtee   r3
  2c:	7c 00 81 46 	wrteei  1
  30:	7c 85 02 06 	mfdcrx  r4,r5
  34:	7c aa 3a 86 	mfdcr   r5,234
  38:	7c e6 03 06 	mtdcrx  r6,r7
  3c:	7d 10 6b 86 	mtdcr   432,r8
  40:	7c 00 04 ac 	sync    
  44:	7c 09 55 ec 	dcba    r9,r10
  48:	7c 00 06 ac 	eieio
