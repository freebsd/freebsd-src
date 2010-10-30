	.text
	.global _start
_start:
	bcl 20,31,1f
1:	mflr 30
	addis 30,30,(_GLOBAL_OFFSET_TABLE_-1b)@ha
	addi 30,30,(_GLOBAL_OFFSET_TABLE_-1b)@l
	bl _exit@plt
	b _start
