#objdump: -dr
#name: TLS
# This test is only valid on ELF based ports.
#not-target: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*
# This is the VxWorks variant of this file.
#source: tls.s
#not-skip: *-*-vxworks*

# Test generation of TLS relocations

.*: +file format .*arm.*

Disassembly of section .text:

00+0 <main>:
   0:	e1a00000 	nop			\(mov r0,r0\)
   4:	e1a00000 	nop			\(mov r0,r0\)
   8:	e1a0f00e 	mov	pc, lr
   c:	00000000 	andeq	r0, r0, r0
			c: R_ARM_TLS_GD32	a
# ??? The addend is appearing in both the RELA field and the
# contents.  Shouldn't it be just one?  bfd_install_relocation
# appears to write the addend into the contents unconditionally,
# yet somehow this does not happen for the majority of relocations.
  10:	00000004 	andeq	r0, r0, r4
			10: R_ARM_TLS_LDM32	b\+0x4
  14:	00000008 	andeq	r0, r0, r8
			14: R_ARM_TLS_IE32	c\+0x8
  18:	00000000 	andeq	r0, r0, r0
			18: R_ARM_TLS_LE32	d
