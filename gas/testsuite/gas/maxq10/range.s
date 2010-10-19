;# checks the 8 bit ranges
;# all negative values should contain a Prefix for MAXQ20
;# immediate values with one operand for MAXQ10 skips PFX
.text 
	move A[0], #-1  
	move Ap, #-1
	move a[0], #1
	move AP, #-125	; AP is an 8 bit register 
	move AP, #-126
	move AP, #-127
	move A[0], #125		; A[0] is an 16 bit register - no pfx req. here
	move A[0], #126
	move A[0], #128
	move A[0], #254	        ; ---------------
	move @++SP, #-1		; check PFX generation for mem operands
	move @++sp, #-126	; -
	move @++sp, #254		; - no pFX here
	move @++sp, #-127	; -
	move @++sp, #-128	;--------------------------
	Add #-1			;Check PFX gen. for single operand instructions
	Add #-127
	Add #-129
	Add #127
	Add #128
	add #129
	add #254
	add #ffh
	add #-254
	add #-127
	add #-129		; --------------------
