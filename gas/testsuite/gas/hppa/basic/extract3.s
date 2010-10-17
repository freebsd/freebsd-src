	.LEVEL 2.0
	.code
	.align 4
; Basic immediate instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	extrd,u,* %r4,10,5,%r6
	extrd,u,*= %r4,10,5,%r6
	extrd,u,*< %r4,10,5,%r6
	extrd,u,*od %r4,10,5,%r6
	extrd,u,*tr %r4,10,5,%r6
	extrd,u,*<> %r4,10,5,%r6
	extrd,u,*>= %r4,10,5,%r6
	extrd,u,*ev %r4,10,5,%r6

	extrd,s,* %r4,10,5,%r6
	extrd,s,*= %r4,10,5,%r6
	extrd,s,*< %r4,10,5,%r6
	extrd,s,*od %r4,10,5,%r6
	extrd,*tr %r4,10,5,%r6
	extrd,*<> %r4,10,5,%r6
	extrd,*>= %r4,10,5,%r6
	extrd,*ev %r4,10,5,%r6

	extrd,u,* %r4,%sar,5,%r6
	extrd,u,*= %r4,%sar,5,%r6
	extrd,u,*< %r4,%sar,5,%r6
	extrd,u,*od %r4,%sar,5,%r6
	extrd,u,*tr %r4,%sar,5,%r6
	extrd,u,*<> %r4,%sar,5,%r6
	extrd,u,*>= %r4,%sar,5,%r6
	extrd,u,*ev %r4,%sar,5,%r6
	
	extrd,s,* %r4,%sar,5,%r6
	extrd,s,*= %r4,%sar,5,%r6
	extrd,s,*< %r4,%sar,5,%r6
	extrd,s,*od %r4,%sar,5,%r6
	extrd,*tr %r4,%sar,5,%r6
	extrd,*<> %r4,%sar,5,%r6
	extrd,*>= %r4,%sar,5,%r6
	extrd,*ev %r4,%sar,5,%r6
