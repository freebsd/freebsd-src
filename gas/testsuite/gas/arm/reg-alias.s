	@ Test case-sensitive register aliases
	.text
	.global fred
fred:
	
MMUPurgeTLBReg      .req c6
MMUCP               .req p15
	
MCR     MMUCP, 0, a1, MMUPurgeTLBReg, c0, 0
	@ The NOPs are here for ports like arm-aout which will pad
	@ the .text section to a 16 byte boundary.
	nop
	nop
	nop
