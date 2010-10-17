# Test fnop's dual bit (all other floating point operations have their dual
# bit tested in their individual test files).

	.text
	.align 8
	nop
	nop
        d.pfadd.dd      %f8,%f10,%f12
        adds	%r5,%r6,%r6
        d.pfadd.dd      %f8,%f10,%f12
        fld.d   16(%r10),%f24
        d.fnop
        fld.d   8(%r10),%f8
        d.fnop
        fld.d   0(%r10),%f16
	nop
	nop
