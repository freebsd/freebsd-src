 .intel_syntax noprefix
 .text
_cbw:
	cbw
	cwde
	cdqe
	rex cbw
	rex cwde
	rex64 cbw
_cwd:
	cwd
	cdq
	cqo
	rex cwd
	rex cdq
	rex64 cwd

	.p2align	4,0
