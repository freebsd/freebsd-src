	.code
	.align 4
	.EXPORT integer_memory_tests,CODE
	.EXPORT integer_indexing_load,CODE
	.EXPORT integer_load_short_memory,CODE
	.EXPORT integer_store_short_memory,CODE
	.EXPORT main,CODE
	.EXPORT main,ENTRY,PRIV_LEV=3,RTNVAL=GR
; Basic integer memory tests which also test the various 
; addressing modes and completers.
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
; 
integer_memory_tests: 
	ldw 0(%sr0,%r4),%r26
	ldh 0(%sr0,%r4),%r26
	ldb 0(%sr0,%r4),%r26
	stw %r26,0(%sr0,%r4)
	sth %r26,0(%sr0,%r4)
	stb %r26,0(%sr0,%r4)

; Should make sure pre/post modes are recognized correctly.
	ldwm 0(%sr0,%r4),%r26
	stwm %r26,0(%sr0,%r4)

integer_indexing_load: 
	ldwx %r5(%sr0,%r4),%r26
	ldwx,s %r5(%sr0,%r4),%r26
	ldwx,m %r5(%sr0,%r4),%r26
	ldwx,sm %r5(%sr0,%r4),%r26
	ldhx %r5(%sr0,%r4),%r26
	ldhx,s %r5(%sr0,%r4),%r26
	ldhx,m %r5(%sr0,%r4),%r26
	ldhx,sm %r5(%sr0,%r4),%r26
	ldbx %r5(%sr0,%r4),%r26
	ldbx,s %r5(%sr0,%r4),%r26
	ldbx,m %r5(%sr0,%r4),%r26
	ldbx,sm %r5(%sr0,%r4),%r26
	ldwax %r5(%r4),%r26
	ldwax,s %r5(%r4),%r26
	ldwax,m %r5(%r4),%r26
	ldwax,sm %r5(%r4),%r26
	ldcwx %r5(%sr0,%r4),%r26
	ldcwx,s %r5(%sr0,%r4),%r26
	ldcwx,m %r5(%sr0,%r4),%r26
	ldcwx,sm %r5(%sr0,%r4),%r26

integer_load_short_memory: 
	ldws 0(%sr0,%r4),%r26
	ldws,mb 0(%sr0,%r4),%r26
	ldws,ma 0(%sr0,%r4),%r26
	ldhs 0(%sr0,%r4),%r26
	ldhs,mb 0(%sr0,%r4),%r26
	ldhs,ma 0(%sr0,%r4),%r26
	ldbs 0(%sr0,%r4),%r26
	ldbs,mb 0(%sr0,%r4),%r26
	ldbs,ma 0(%sr0,%r4),%r26
	ldwas 0(%r4),%r26
	ldwas,mb 0(%r4),%r26
	ldwas,ma 0(%r4),%r26
	ldcws 0(%sr0,%r4),%r26
	ldcws,mb 0(%sr0,%r4),%r26
	ldcws,ma 0(%sr0,%r4),%r26

integer_store_short_memory: 
	stws %r26,0(%sr0,%r4)
	stws,mb %r26,0(%sr0,%r4)
	stws,ma %r26,0(%sr0,%r4)
	sths %r26,0(%sr0,%r4)
	sths,mb %r26,0(%sr0,%r4)
	sths,ma %r26,0(%sr0,%r4)
	stbs %r26,0(%sr0,%r4)
	stbs,mb %r26,0(%sr0,%r4)
	stbs,ma %r26,0(%sr0,%r4)
	stwas %r26,0(%r4)
	stwas,mb %r26,0(%r4)
	stwas,ma %r26,0(%r4)
	stbys %r26,0(%sr0,%r4)
	stbys,b %r26,0(%sr0,%r4)
	stbys,e %r26,0(%sr0,%r4)
	stbys,b,m %r26,0(%sr0,%r4)
	stbys,e,m %r26,0(%sr0,%r4)
