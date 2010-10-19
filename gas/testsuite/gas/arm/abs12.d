#objdump: -dr
#not-skip: *-vxworks

.*:     file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	e5910000 	ldr	r0, \[r1\]
			0: R_ARM_ABS12	global
   4:	e5910000 	ldr	r0, \[r1\]
			4: R_ARM_ABS12	global\+0xc
   8:	e5910000 	ldr	r0, \[r1\]
			8: R_ARM_ABS12	global\+0x100000
   c:	e5910000 	ldr	r0, \[r1\]
			c: R_ARM_ABS12	\.text\+0x18
  10:	e5910000 	ldr	r0, \[r1\]
			10: R_ARM_ABS12	\.text\+0x24
  14:	e5910000 	ldr	r0, \[r1\]
			14: R_ARM_ABS12	\.text\+0x100018
