#as:
#objdump: -Dr
#name: frv fdpic

.*: +file format .*

Disassembly of section \.text:

00000000 <foo>:
   0:	a0 50 f0 00 	subi gr15,0,gr16
			0: R_FRV_GPREL12	bar
   4:	88 40 f0 00 	addi gr15,0,gr4
			4: R_FRV_GOT12	foo
   8:	8a c8 f0 00 	ldi @\(gr15,0\),gr5
			8: R_FRV_GOT12	foo
   c:	8c f4 00 00 	setlo lo\(0x0\),gr6
			c: R_FRV_GOTLO	foo
  10:	8c f8 00 00 	sethi hi\(0x0\),gr6
			10: R_FRV_GOTHI	foo
  14:	8e 40 f0 00 	addi gr15,0,gr7
			14: R_FRV_FUNCDESC_GOT12	foo
  18:	90 c8 f0 00 	ldi @\(gr15,0\),gr8
			18: R_FRV_FUNCDESC_GOT12	foo
  1c:	92 f4 00 00 	setlo lo\(0x0\),gr9
			1c: R_FRV_FUNCDESC_GOTLO	foo
  20:	92 f8 00 00 	sethi hi\(0x0\),gr9
			20: R_FRV_FUNCDESC_GOTHI	foo
  24:	a0 40 f0 00 	addi gr15,0,gr16
			24: R_FRV_GOTOFF12	\.sdata
  28:	88 40 f0 00 	addi gr15,0,gr4
			28: R_FRV_GOTOFF12	foo
  2c:	8a c8 f0 00 	ldi @\(gr15,0\),gr5
			2c: R_FRV_GOTOFF12	foo
  30:	8c f4 00 00 	setlo lo\(0x0\),gr6
			30: R_FRV_GOTOFFLO	foo
  34:	8c f8 00 00 	sethi hi\(0x0\),gr6
			34: R_FRV_GOTOFFHI	foo
  38:	8e 40 f0 00 	addi gr15,0,gr7
			38: R_FRV_FUNCDESC_GOTOFF12	foo
  3c:	90 c8 f0 00 	ldi @\(gr15,0\),gr8
			3c: R_FRV_FUNCDESC_GOTOFF12	foo
  40:	92 f4 00 00 	setlo lo\(0x0\),gr9
			40: R_FRV_FUNCDESC_GOTOFFLO	foo
  44:	92 f8 00 00 	sethi hi\(0x0\),gr9
			44: R_FRV_FUNCDESC_GOTOFFHI	foo
Disassembly of section \.sdata:

00000000 <baz>:
	\.\.\.
			0: R_FRV_FUNCDESC	foo
			4: R_FRV_32	foo
