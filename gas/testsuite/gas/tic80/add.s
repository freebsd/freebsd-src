; Test signed and unsigned addition instruction.
; Test boundary conditions to ensure proper handling.
; Note that unsigned addition still uses signed immediates.

	add	r10,r11,r12		; Register form
	add	16383,r2,r4		; Maximum positive short signed immediate
	add	-16384,r4,r4		; Minimum negative short signed immediate
	add	16384,r5,r6		; Minimum positive long signed immediate
	add	-16385,r7,r8		; Maximum negative long signed immediate
	add	2147483647,r10,r11	; Maximum positive long signed immediate
	add	-2147483648,r12,r13	; Minimum negative long signed immediate

	addu	r10,r11,r12		; Register form
	addu	16383,r2,r4		; Maximum positive short signed immediate
	addu	-16384,r4,r4		; Minimum negative short signed immediate
	addu	16384,r5,r6		; Minimum positive long signed immediate
	addu	-16385,r7,r8		; Maximum negative long signed immediate
	addu	2147483647,r10,r11	; Maximum positive long signed immediate
	addu	-2147483648,r12,r13	; Minimum negative long signed immediate
