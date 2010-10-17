// Intel assembler directives:
// Test that the .dual and .enddual directives are recognized and
// function (i.e., that the dual bits are set properly).

	.text

	nop
	nop
	.dual
        fadd.ss       f0,f1,f2
        nop
        fadd.sd       f2,f3,f4
        nop
        fadd.dd       f6,f8,f10
        nop
	.enddual
	nop
	nop

