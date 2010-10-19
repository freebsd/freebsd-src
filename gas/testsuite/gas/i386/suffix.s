# Disassembling with -Msuffix.

	.text
foo:
	monitor
	mwait

	vmcall
	vmlaunch
	vmresume
	vmxoff

	.p2align	4,0
