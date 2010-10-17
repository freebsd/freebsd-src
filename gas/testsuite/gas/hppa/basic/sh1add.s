	.code
	.align 4
; Basic immediate instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	sh1add  %r4,%r5,%r6
	sh1add,=  %r4,%r5,%r6
	sh1add,<  %r4,%r5,%r6
	sh1add,<=  %r4,%r5,%r6
	sh1add,nuv  %r4,%r5,%r6
	sh1add,znv  %r4,%r5,%r6
	sh1add,sv  %r4,%r5,%r6
	sh1add,od  %r4,%r5,%r6
	sh1add,tr  %r4,%r5,%r6
	sh1add,<>  %r4,%r5,%r6
	sh1add,>=  %r4,%r5,%r6
	sh1add,>  %r4,%r5,%r6
	sh1add,uv  %r4,%r5,%r6
	sh1add,vnz  %r4,%r5,%r6
	sh1add,nsv  %r4,%r5,%r6
	sh1add,ev  %r4,%r5,%r6

	sh1addl  %r4,%r5,%r6
	sh1addl,=  %r4,%r5,%r6
	sh1addl,<  %r4,%r5,%r6
	sh1addl,<=  %r4,%r5,%r6
	sh1addl,nuv  %r4,%r5,%r6
	sh1addl,znv  %r4,%r5,%r6
	sh1addl,sv  %r4,%r5,%r6
	sh1addl,od  %r4,%r5,%r6
	sh1addl,tr  %r4,%r5,%r6
	sh1addl,<>  %r4,%r5,%r6
	sh1addl,>=  %r4,%r5,%r6
	sh1addl,>  %r4,%r5,%r6
	sh1addl,uv  %r4,%r5,%r6
	sh1addl,vnz  %r4,%r5,%r6
	sh1addl,nsv  %r4,%r5,%r6
	sh1addl,ev  %r4,%r5,%r6

	sh1addo  %r4,%r5,%r6
	sh1addo,=  %r4,%r5,%r6
	sh1addo,<  %r4,%r5,%r6
	sh1addo,<=  %r4,%r5,%r6
	sh1addo,nuv  %r4,%r5,%r6
	sh1addo,znv  %r4,%r5,%r6
	sh1addo,sv  %r4,%r5,%r6
	sh1addo,od  %r4,%r5,%r6
	sh1addo,tr  %r4,%r5,%r6
	sh1addo,<>  %r4,%r5,%r6
	sh1addo,>=  %r4,%r5,%r6
	sh1addo,>  %r4,%r5,%r6
	sh1addo,uv  %r4,%r5,%r6
	sh1addo,vnz  %r4,%r5,%r6
	sh1addo,nsv  %r4,%r5,%r6
	sh1addo,ev  %r4,%r5,%r6

