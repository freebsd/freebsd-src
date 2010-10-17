	.h8300s
	.text
h8300s_mac:
	clrmac
	ldmac er4,mach
	ldmac er5,macl
	mac @er4+,@er5+
	stmac mach,er4
	stmac macl,er5
	

