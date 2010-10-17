#NO_APP
gcc2_compiled.:
___gnu_compiled_c:
.text
	.even
.globl _foo
_foo:
	link %a6,#-12
	fmovex %a6@(-12),%fp0
	fmovex %fp0,%sp@-
	jbsr _bar
	addqw #8,%sp
	addqw #4,%sp
L1:
	unlk %a6
	rts
