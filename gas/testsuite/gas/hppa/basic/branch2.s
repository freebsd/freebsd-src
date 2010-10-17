	.LEVEL 2.0
	.code
	.align 4
; More branching instructions than you ever knew what to do with.
; PA 2.0 versions and new syntax.

;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.

bb_tests:	
	bb,< %r4,%sar,bb_tests
	bb,>= %r4,%sar,bb_tests
	bb,<,n %r4,%cr11,bb_tests
	bb,>=,n %r4,%cr11,bb_tests
	bb,*< %r4,%sar,bb_tests
	bb,*>= %r4,%sar,bb_tests
	bb,*<,n %r4,%cr11,bb_tests
	bb,*>=,n %r4,%cr11,bb_tests
	bb,*< %r4,5,bb_tests
	bb,*>= %r4,5,bb_tests
	bb,*<,n %r4,5,bb_tests
	bb,*>=,n %r4,5,bb_tests
	
branch_stack:	
	clrbts
	popbts 1
	popbts 499
	pushnom
	pushbts %r4

