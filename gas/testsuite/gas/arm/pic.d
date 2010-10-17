#objdump: -dr --prefix-addresses --show-raw-insn
#name: PIC

# Test generation of PIC

.*: +file format .*arm.*

Disassembly of section .text:
00+0 <[^>]*> ebfffffe 	bl	00+0 <[^>]*>
			0: R_ARM_PC24	foo
00+4 <[^>]*> ebfffffe 	bl	00+4 <[^>]*>
			4: R_ARM_PLT32	foo
	\.\.\.
			8: R_ARM_ABS32	sym
			c: R_ARM_GOT32	sym
			10: R_ARM_GOTOFF	sym
			14: R_ARM_GOTPC	_GLOBAL_OFFSET_TABLE_
