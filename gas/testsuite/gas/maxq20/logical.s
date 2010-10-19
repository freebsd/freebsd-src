;# logical.s
;# Verifies all the logical operation in the file

.text	
foo:
	MOVE AP, #00h	;Set AC[0] as the active accumulator
	AND #FFh	;AND AC[0] with 0xFF
	OR #F0h
	XOR #FEh
	CPL
	NEG
	SLA
	SLA2	
	SLA4
	RL	
	RLC
	SRA
	SRA2
	SRA4
	SR
	RR
	RRC
