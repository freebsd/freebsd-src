	.code
	.align 4
; Basic copr memory tests which also test the various 
; addressing modes and completers.
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
; 
copr_indexing_load:

	cldwx,4 %r5(%sr0,%r4),%r26
	cldwx,4,s %r5(%sr0,%r4),%r26
	cldwx,4,m %r5(%sr0,%r4),%r26
	cldwx,4,sm %r5(%sr0,%r4),%r26
	clddx,4 %r5(%sr0,%r4),%r26
	clddx,4,s %r5(%sr0,%r4),%r26
	clddx,4,m %r5(%sr0,%r4),%r26
	clddx,4,sm %r5(%sr0,%r4),%r26

copr_indexing_store:
	cstwx,4 %r26,%r5(%sr0,%r4)
	cstwx,4,s %r26,%r5(%sr0,%r4)
	cstwx,4,m %r26,%r5(%sr0,%r4)
	cstwx,4,sm %r26,%r5(%sr0,%r4)
	cstdx,4 %r26,%r5(%sr0,%r4)
	cstdx,4,s %r26,%r5(%sr0,%r4)
	cstdx,4,m %r26,%r5(%sr0,%r4)
	cstdx,4,sm %r26,%r5(%sr0,%r4)

copr_short_memory:
	cldws,4 0(%sr0,%r4),%r26
	cldws,4,mb 0(%sr0,%r4),%r26
	cldws,4,ma 0(%sr0,%r4),%r26
	cldds,4 0(%sr0,%r4),%r26
	cldds,4,mb 0(%sr0,%r4),%r26
	cldds,4,ma 0(%sr0,%r4),%r26
	cstws,4 %r26,0(%sr0,%r4)
	cstws,4,mb %r26,0(%sr0,%r4)
	cstws,4,ma %r26,0(%sr0,%r4)
	cstds,4 %r26,0(%sr0,%r4)
	cstds,4,mb %r26,0(%sr0,%r4)
	cstds,4,ma %r26,0(%sr0,%r4)

; gas fucks this up thinks it gets the expression 4 modulo 5
;	cldwx,4 %r5(0,%r4),%r%r26
