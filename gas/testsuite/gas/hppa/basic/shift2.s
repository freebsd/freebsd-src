	.code
	.align 4
; PA 2.0 format shift right instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	shrpw %r4,%r5,%sar,%r6
	shrpw,= %r4,%r5,%sar,%r6
	shrpw,< %r4,%r5,%sar,%r6
	shrpw,od %r4,%r5,%sar,%r6
	shrpw,tr %r4,%r5,%cr11,%r6
	shrpw,<> %r4,%r5,%cr11,%r6
	shrpw,>= %r4,%r5,%cr11,%r6
	shrpw,ev %r4,%r5,%cr11,%r6

	shrpw %r4,%r5,5,%r6
	shrpw,= %r4,%r5,5,%r6
	shrpw,< %r4,%r5,5,%r6
	shrpw,od %r4,%r5,5,%r6
	shrpw,tr %r4,%r5,5,%r6
	shrpw,<> %r4,%r5,5,%r6
	shrpw,>= %r4,%r5,5,%r6
	shrpw,ev %r4,%r5,5,%r6
