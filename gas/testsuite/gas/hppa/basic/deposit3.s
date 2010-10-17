	.LEVEL 2.0
	.code
	.align 4
; PA 2.0 Deposit instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	depd,z,* %r4,10,5,%r6
	depd,z,*= %r4,10,5,%r6
	depd,z,*< %r4,10,5,%r6
	depd,z,*od %r4,10,5,%r6
	depd,z,*tr %r4,10,5,%r6
	depd,z,*<> %r4,10,5,%r6
	depd,z,*>= %r4,10,5,%r6
	depd,z,*ev %r4,10,5,%r6

	depd,* %r4,10,5,%r6
	depd,*= %r4,10,5,%r6
	depd,*< %r4,10,5,%r6
	depd,*od %r4,10,5,%r6
	depd,*tr %r4,10,5,%r6
	depd,*<> %r4,10,5,%r6
	depd,*>= %r4,10,5,%r6
	depd,*ev %r4,10,5,%r6

	depd,z,* %r4,%sar,5,%r6
	depd,z,*= %r4,%sar,5,%r6
	depd,z,*< %r4,%sar,5,%r6
	depd,z,*od %r4,%sar,5,%r6
	depd,z,*tr %r4,%sar,5,%r6
	depd,z,*<> %r4,%sar,5,%r6
	depd,z,*>= %r4,%sar,5,%r6
	depd,z,*ev %r4,%sar,5,%r6

	depd,* %r4,%sar,5,%r6
	depd,*= %r4,%sar,5,%r6
	depd,*< %r4,%sar,5,%r6
	depd,*od %r4,%sar,5,%r6
	depd,*tr %r4,%sar,5,%r6
	depd,*<> %r4,%sar,5,%r6
	depd,*>= %r4,%sar,5,%r6
	depd,*ev %r4,%sar,5,%r6

	depdi,* -1,%sar,5,%r6
	depdi,*= -1,%sar,5,%r6
	depdi,*< -1,%sar,5,%r6
	depdi,*od -1,%sar,5,%r6
	depdi,*tr -1,%sar,5,%r6
	depdi,*<> -1,%sar,5,%r6
	depdi,*>= -1,%sar,5,%r6
	depdi,*ev -1,%sar,5,%r6

	depdi,z,* -1,%sar,5,%r6
	depdi,z,*= -1,%sar,5,%r6
	depdi,z,*< -1,%sar,5,%r6
	depdi,z,*od -1,%sar,5,%r6
	depdi,z,*tr -1,%sar,5,%r6
	depdi,z,*<> -1,%sar,5,%r6
	depdi,z,*>= -1,%sar,5,%r6
	depdi,z,*ev -1,%sar,5,%r6

	depdi,* -1,10,4,%r6
	depdi,*= -1,10,4,%r6
	depdi,*< -1,10,4,%r6
	depdi,*od -1,10,4,%r6
	depdi,*tr -1,10,4,%r6
	depdi,*<> -1,10,4,%r6
	depdi,*>= -1,10,4,%r6
	depdi,*ev -1,10,4,%r6

	depdi,z,* -1,10,4,%r6
	depdi,z,*= -1,10,4,%r6
	depdi,z,*< -1,10,4,%r6
	depdi,z,*od -1,10,4,%r6
	depdi,z,*tr -1,10,4,%r6
	depdi,z,*<> -1,10,4,%r6
	depdi,z,*>= -1,10,4,%r6
	depdi,z,*ev -1,10,4,%r6
