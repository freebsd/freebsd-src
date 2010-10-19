; Offset less than #-32786 should cause an error since the offset is
; a signed quantity.  Also tests expression parsing.

label1: add R1,R2,R3
label2:	add R4,R5,R6
	brlt R7,R8, ((label1-label2)-32765) ; evaluates to -32769
