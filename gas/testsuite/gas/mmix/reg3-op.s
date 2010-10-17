# All-registers, '3'-type operands; third operand is
# register or constant.
Main	MUL X,Y,Z
	ADD $32,Y,Z
	4ADDU Y,$32,Z
	8ADDU $232,$133,Z
	16ADDU X,Y,$73
	SRU $31,Y,$233
	CSN X,$38,$212
	ZSNP $4,$175,$181

	MULU X,Y,Z0
	SL $32,Y,Z0
	CMPU Y,$32,Z0
	2ADDU $232,$133,Z0
	MXOR X,Y,203
	OR $31,Y,213
	NAND X,$38,211
	WDIF $4,$175,161

	SADD X,Y,0
	MXOR $32,Y,0
	ORN Y,$32,0
	ANDN $232,$133,0
	16ADDU X,Y,0
	SL $31,Y,0
	ADDU X,$38,0
	CMP $4,$175,0
X IS $23
Y IS $12
Z IS $67
Z0 IS 176
