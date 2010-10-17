	.code
	.align 4
; Basic immediate instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	ds %r4,%r5,%r6
	ds,= %r4,%r5,%r6
	ds,< %r4,%r5,%r6
	ds,<= %r4,%r5,%r6
	ds,<< %r4,%r5,%r6
	ds,<<= %r4,%r5,%r6
	ds,sv %r4,%r5,%r6
	ds,od %r4,%r5,%r6
	ds,tr %r4,%r5,%r6
	ds,<> %r4,%r5,%r6
	ds,>= %r4,%r5,%r6
	ds,> %r4,%r5,%r6
	ds,>>= %r4,%r5,%r6
	ds,>> %r4,%r5,%r6
	ds,nsv %r4,%r5,%r6
	ds,ev %r4,%r5,%r6
