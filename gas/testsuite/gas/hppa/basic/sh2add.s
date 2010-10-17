	.code
	.align 4
; Basic immediate instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	sh2add  %r4,%r5,%r6
	sh2add,=  %r4,%r5,%r6
	sh2add,<  %r4,%r5,%r6
	sh2add,<=  %r4,%r5,%r6
	sh2add,nuv  %r4,%r5,%r6
	sh2add,znv  %r4,%r5,%r6
	sh2add,sv  %r4,%r5,%r6
	sh2add,od  %r4,%r5,%r6
	sh2add,tr  %r4,%r5,%r6
	sh2add,<>  %r4,%r5,%r6
	sh2add,>=  %r4,%r5,%r6
	sh2add,>  %r4,%r5,%r6
	sh2add,uv  %r4,%r5,%r6
	sh2add,vnz  %r4,%r5,%r6
	sh2add,nsv  %r4,%r5,%r6
	sh2add,ev  %r4,%r5,%r6

	sh2addl  %r4,%r5,%r6
	sh2addl,=  %r4,%r5,%r6
	sh2addl,<  %r4,%r5,%r6
	sh2addl,<=  %r4,%r5,%r6
	sh2addl,nuv  %r4,%r5,%r6
	sh2addl,znv  %r4,%r5,%r6
	sh2addl,sv  %r4,%r5,%r6
	sh2addl,od  %r4,%r5,%r6
	sh2addl,tr  %r4,%r5,%r6
	sh2addl,<>  %r4,%r5,%r6
	sh2addl,>=  %r4,%r5,%r6
	sh2addl,>  %r4,%r5,%r6
	sh2addl,uv  %r4,%r5,%r6
	sh2addl,vnz  %r4,%r5,%r6
	sh2addl,nsv  %r4,%r5,%r6
	sh2addl,ev  %r4,%r5,%r6

	sh2addo  %r4,%r5,%r6
	sh2addo,=  %r4,%r5,%r6
	sh2addo,<  %r4,%r5,%r6
	sh2addo,<=  %r4,%r5,%r6
	sh2addo,nuv  %r4,%r5,%r6
	sh2addo,znv  %r4,%r5,%r6
	sh2addo,sv  %r4,%r5,%r6
	sh2addo,od  %r4,%r5,%r6
	sh2addo,tr  %r4,%r5,%r6
	sh2addo,<>  %r4,%r5,%r6
	sh2addo,>=  %r4,%r5,%r6
	sh2addo,>  %r4,%r5,%r6
	sh2addo,uv  %r4,%r5,%r6
	sh2addo,vnz  %r4,%r5,%r6
	sh2addo,nsv  %r4,%r5,%r6
	sh2addo,ev  %r4,%r5,%r6

