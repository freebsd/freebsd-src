#objdump: -dr --prefix-addresses --show-raw-insn
#name: PIC
# This test is only valid on ELF based ports.
#not-target: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*
# VxWorks needs a special variant of this file.
#skip: *-*-vxworks*

# Test generation of PIC

.*: +file format .*arm.*

Disassembly of section .text:
00+0 <[^>]*> eb...... 	bl	00+. <[^>]*>
			0: R_ARM_(PC24|CALL)	foo.*
00+4 <[^>]*> eb...... 	bl	0[0123456789abcdef]+ <[^>]*>
			4: R_ARM_PLT32	foo
	\.\.\.
			8: R_ARM_ABS32	sym
			c: R_ARM_GOT32	sym
			10: R_ARM_GOTOFF32	sym
			14: R_ARM_GOTPC	_GLOBAL_OFFSET_TABLE_
			18: R_ARM_TARGET1	foo2
			1c: R_ARM_SBREL32	foo3
			20: R_ARM_TARGET2	foo4
