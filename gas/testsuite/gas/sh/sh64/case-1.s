! Tests that opcodes and common registers are recognized case-insensitive,
! and also that the option --isa=shmedia is optional.

	.mode SHmedia
	.text
start:
	nOp
	NOP
	pt/U foo,tr4
	PTA/l bar,Tr3
	MOVI 42,R2
	PTA/L start,TR2
