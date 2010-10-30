	.globl		Gdefault
	.globl		Ghidden
	.globl		Ginternal
	.globl		Gprotected

	.weak		Wdefault
	.weak		Whidden
	.weak		Winternal
	.weak		Wprotected

	.hidden		Lhidden
	.hidden		Ghidden
	.hidden		Whidden

	.internal	Linternal
	.internal	Ginternal
	.internal	Winternal

	.protected	Lprotected
	.protected	Gprotected
	.protected	Wprotected

	.equ		Ldefault, 0x1100
	.equ		Lhidden, 0x1200
	.equ		Linternal, 0x1300
	.equ		Lprotected, 0x1400

	.equ		Gdefault, 0x2100
	.equ		Ghidden, 0x2200
	.equ		Ginternal, 0x2300
	.equ		Gprotected, 0x2400

	.equ		Wdefault, 0x3100
	.equ		Whidden, 0x3200
	.equ		Winternal, 0x3300
	.equ		Wprotected, 0x3400
