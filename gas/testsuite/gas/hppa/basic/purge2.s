	.level 2.0
	.code
	.align 4
; PA2.0 purge instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	pdtlb,l %r4(%sr0,%r5)
	pdtlb,l,m %r4(%sr0,%r5)
	pitlb,l %r4(%sr4,%r5)
	pitlb,l,m %r4(%sr4,%r5)
