#objdump: -dr --prefix-addresses --show-raw-insn
#name: PIC
#source: pic.s
#not-skip: *-*-vxworks*

# Test generation of PIC

.*: +file format .*arm.*

Disassembly of section .text:
00+0 <[^>]*> eb000000 	bl	.*
			0: R_ARM_PC24	foo\+0xfffffff8
00+4 <[^>]*> eb000000 	bl	.*
			4: R_ARM_PLT32	foo\+0xfffffff8
	\.\.\.
			8: R_ARM_ABS32	sym
			c: R_ARM_GOT32	sym
			10: R_ARM_GOTOFF32	sym
			14: R_ARM_GOTPC	_GLOBAL_OFFSET_TABLE_
			18: R_ARM_TARGET1	foo2
			1c: R_ARM_SBREL32	foo3
			20: R_ARM_TARGET2	foo4
