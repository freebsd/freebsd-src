#objdump: -dr
#name: TLS
# This test is only valid on ELF based ports.
#not-target: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*
# VxWorks needs a special variant of this file.
#skip: *-*-vxworks*

# Test generation of TLS relocations

.*: +file format .*arm.*

Disassembly of section .text:

00+0 <main>:
   0:	e1a00000 	nop			\(mov r0,r0\)
   4:	e1a00000 	nop			\(mov r0,r0\)
   8:	e1a0f00e 	mov	pc, lr
   c:	00000000 	.word	0x00000000
			c: R_ARM_TLS_GD32	a
  10:	00000004 	.word	0x00000004
			10: R_ARM_TLS_LDM32	b
  14:	00000008 	.word	0x00000008
			14: R_ARM_TLS_IE32	c
  18:	00000000 	.word	0x00000000
			18: R_ARM_TLS_LE32	d
