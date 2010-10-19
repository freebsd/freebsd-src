;# math.s
;# Implements all the math intuctions

.text
foo:	
	ADD #01h	; add 01h to accumulator
	ADD #02h
	ADD #03h
	ADD #04h
	ADD #05h
	ADD A[0]	; Add Active accumulator+A[0]
	ADD A[1]
	ADD A[2]
	ADD A[3]
	ADD A[4]
	ADDC #31h
	ADDC #32h
	ADDC #33h
	ADDC A[0]
	ADDC A[1]
	ADDC A[2]
	ADDC A[3]
	SUB #01h	; Substract 01h from accumulator
	SUB #02h
	SUB #03h
	SUB #04h
	SUB #05h
	SUB A[0]	; Active accumulator-A[0]
	SUB A[1]
	SUB A[2]
	SUB A[3]
	SUB A[4]	
	SUBB #31h
	SUBB #32h
	SUBB #33h
	SUBB A[0]
	SUBB A[1]
	SUBB A[2]
	SUBB A[3]
