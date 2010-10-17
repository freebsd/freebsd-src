	.code
	.align 4
; Deposit instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	depw,z %r4,5,10,%r6
	depw,z,= %r4,5,10,%r6
	depw,z,< %r4,5,10,%r6
	depw,z,od %r4,5,10,%r6
	depw,z,tr %r4,5,10,%r6
	depw,z,<> %r4,5,10,%r6
	depw,z,>= %r4,5,10,%r6
	depw,z,ev %r4,5,10,%r6

	depw %r4,5,10,%r6
	depw,= %r4,5,10,%r6
	depw,< %r4,5,10,%r6
	depw,od %r4,5,10,%r6
	depw,tr %r4,5,10,%r6
	depw,<> %r4,5,10,%r6
	depw,>= %r4,5,10,%r6
	depw,ev %r4,5,10,%r6

	depw,z	%r4,%sar,5,%r6
	depw,z,= %r4,%sar,5,%r6
	depw,z,< %r4,%sar,5,%r6
	depw,z,od %r4,%sar,5,%r6
	depw,z,tr %r4,%sar,5,%r6
	depw,z,<> %r4,%sar,5,%r6
	depw,z,>= %r4,%sar,5,%r6
	depw,z,ev %r4,%sar,5,%r6

	depw %r4,%sar,5,%r6
	depw,= %r4,%sar,5,%r6
	depw,< %r4,%sar,5,%r6
	depw,od %r4,%sar,5,%r6
	depw,tr %r4,%sar,5,%r6
	depw,<> %r4,%sar,5,%r6
	depw,>= %r4,%sar,5,%r6
	depw,ev %r4,%sar,5,%r6

	depwi -1,%sar,5,%r6
	depwi,= -1,%sar,5,%r6
	depwi,< -1,%sar,5,%r6
	depwi,od -1,%sar,5,%r6
	depwi,tr -1,%sar,5,%r6
	depwi,<> -1,%sar,5,%r6
	depwi,>= -1,%sar,5,%r6
	depwi,ev -1,%sar,5,%r6

	depwi,z -1,%sar,5,%r6
	depwi,z,= -1,%sar,5,%r6
	depwi,z,< -1,%sar,5,%r6
	depwi,z,od -1,%sar,5,%r6
	depwi,z,tr -1,%sar,5,%r6
	depwi,z,<> -1,%sar,5,%r6
	depwi,z,>= -1,%sar,5,%r6
	depwi,z,ev -1,%sar,5,%r6

	depwi -1,4,10,%r6
	depwi,= -1,4,10,%r6
	depwi,< -1,4,10,%r6
	depwi,od -1,4,10,%r6
	depwi,tr -1,4,10,%r6
	depwi,<> -1,4,10,%r6
	depwi,>= -1,4,10,%r6
	depwi,ev -1,4,10,%r6

	depwi,z	-1,4,10,%r6
	depwi,z,= -1,4,10,%r6
	depwi,z,< -1,4,10,%r6
	depwi,z,od -1,4,10,%r6
	depwi,z,tr -1,4,10,%r6
	depwi,z,<> -1,4,10,%r6
	depwi,z,>= -1,4,10,%r6
	depwi,z,ev -1,4,10,%r6
