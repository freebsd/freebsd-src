.text
|*****************************************************************
| Test all permutations of mov3q
|*****************************************************************
	.global test_mov3q
test_mov3q:
	mov3q.l	#-1,%d0			| Mode 0
	mov3q.l #1,%a1			| Mode 1
	mov3q.l #2,(%a2)		| Mode 2
	mov3q.l #3,(%a3)+		| Mode 3
	mov3q.l #4,-(%a4)		| Mode 4
	mov3q.l #5,(1234,%a5)		| Mode 5
	mov3q.l #6,(3,%a6,%d6)		| Mode 6
	mov3q.l #7,0x1234.w		| Mode 7.0
	mov3q.l #-1,0x12345678.l	| Mode 7.1

