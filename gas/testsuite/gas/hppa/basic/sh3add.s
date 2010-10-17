	.code
	.align 4
; Basic immediate instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	sh3add  %r4,%r5,%r6
	sh3add,=  %r4,%r5,%r6
	sh3add,<  %r4,%r5,%r6
	sh3add,<=  %r4,%r5,%r6
	sh3add,nuv  %r4,%r5,%r6
	sh3add,znv  %r4,%r5,%r6
	sh3add,sv  %r4,%r5,%r6
	sh3add,od  %r4,%r5,%r6
	sh3add,tr  %r4,%r5,%r6
	sh3add,<>  %r4,%r5,%r6
	sh3add,>=  %r4,%r5,%r6
	sh3add,>  %r4,%r5,%r6
	sh3add,uv  %r4,%r5,%r6
	sh3add,vnz  %r4,%r5,%r6
	sh3add,nsv  %r4,%r5,%r6
	sh3add,ev  %r4,%r5,%r6

	sh3addl  %r4,%r5,%r6
	sh3addl,=  %r4,%r5,%r6
	sh3addl,<  %r4,%r5,%r6
	sh3addl,<=  %r4,%r5,%r6
	sh3addl,nuv  %r4,%r5,%r6
	sh3addl,znv  %r4,%r5,%r6
	sh3addl,sv  %r4,%r5,%r6
	sh3addl,od  %r4,%r5,%r6
	sh3addl,tr  %r4,%r5,%r6
	sh3addl,<>  %r4,%r5,%r6
	sh3addl,>=  %r4,%r5,%r6
	sh3addl,>  %r4,%r5,%r6
	sh3addl,uv  %r4,%r5,%r6
	sh3addl,vnz  %r4,%r5,%r6
	sh3addl,nsv  %r4,%r5,%r6
	sh3addl,ev  %r4,%r5,%r6

	sh3addo  %r4,%r5,%r6
	sh3addo,=  %r4,%r5,%r6
	sh3addo,<  %r4,%r5,%r6
	sh3addo,<=  %r4,%r5,%r6
	sh3addo,nuv  %r4,%r5,%r6
	sh3addo,znv  %r4,%r5,%r6
	sh3addo,sv  %r4,%r5,%r6
	sh3addo,od  %r4,%r5,%r6
	sh3addo,tr  %r4,%r5,%r6
	sh3addo,<>  %r4,%r5,%r6
	sh3addo,>=  %r4,%r5,%r6
	sh3addo,>  %r4,%r5,%r6
	sh3addo,uv  %r4,%r5,%r6
	sh3addo,vnz  %r4,%r5,%r6
	sh3addo,nsv  %r4,%r5,%r6
	sh3addo,ev  %r4,%r5,%r6

