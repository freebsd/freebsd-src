;# calls.s
;# check program flow instruction involving CALL & RET
.text
foo:
	Call SmallCall
	Call LongCall
SmallCall:
	RET
	RET C
	RET Z
	RET NZ
	RET S	
	RETI
	RETI C
	RETI Z
	RETI NZ
	RETI S
	MOVE LC[1], #10h
LoopTop:
	Call LoopTop
	DJNZ LC[1], LoopTop
	MOVE LC[1], #10h
LoopTop1:
	Call LoopTop1
	.fill 0x200, 2, 0 
	DJNZ LC[1], LoopTop	
LongCall:
	RETI
	RETI C
	RETI Z
	RETI NZ
	RETI S
