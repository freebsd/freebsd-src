	.LEVEL 2.0
	.code
	.align 4
; PA 2.0 shift right instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	shrpd,* %r4,%r5,%sar,%r6
	shrpd,*= %r4,%r5,%sar,%r6
	shrpd,*< %r4,%r5,%sar,%r6
	shrpd,*od %r4,%r5,%sar,%r6
	shrpd,*tr %r4,%r5,%cr11,%r6
	shrpd,*<> %r4,%r5,%cr11,%r6
	shrpd,*>= %r4,%r5,%cr11,%r6
	shrpd,*ev %r4,%r5,%cr11,%r6

	shrpd,* %r4,%r5,5,%r6
	shrpd,*= %r4,%r5,5,%r6
	shrpd,*< %r4,%r5,5,%r6
	shrpd,*od %r4,%r5,5,%r6
	shrpd,*tr %r4,%r5,5,%r6
	shrpd,*<> %r4,%r5,5,%r6
	shrpd,*>= %r4,%r5,5,%r6
	shrpd,*ev %r4,%r5,5,%r6

