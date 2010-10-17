	.code
	.align 4
; Basic immediate instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	subi 123,%r5,%r6
	subi,= 123,%r5,%r6
	subi,< 123,%r5,%r6
	subi,<= 123,%r5,%r6
	subi,<< 123,%r5,%r6
	subi,<<= 123,%r5,%r6
	subi,sv 123,%r5,%r6
	subi,od 123,%r5,%r6
	subi,tr 123,%r5,%r6
	subi,<> 123,%r5,%r6
	subi,>= 123,%r5,%r6
	subi,> 123,%r5,%r6
	subi,>>= 123,%r5,%r6
	subi,>> 123,%r5,%r6
	subi,nsv 123,%r5,%r6
	subi,ev 123,%r5,%r6

	subio 123,%r5,%r6
	subio,= 123,%r5,%r6
	subio,< 123,%r5,%r6
	subio,<= 123,%r5,%r6
	subio,<< 123,%r5,%r6
	subio,<<= 123,%r5,%r6
	subio,sv 123,%r5,%r6
	subio,od 123,%r5,%r6
	subio,tr 123,%r5,%r6
	subio,<> 123,%r5,%r6
	subio,>= 123,%r5,%r6
	subio,> 123,%r5,%r6
	subio,>>= 123,%r5,%r6
	subio,>> 123,%r5,%r6
	subio,nsv 123,%r5,%r6
	subio,ev 123,%r5,%r6

	subi,tsv 123,%r5,%r6
	subi,tsv,= 123,%r5,%r6
	subi,tsv,< 123,%r5,%r6
	subi,tsv,<= 123,%r5,%r6
	subi,tsv,<< 123,%r5,%r6
	subi,tsv,<<= 123,%r5,%r6
	subi,tsv,sv 123,%r5,%r6
	subi,tsv,od 123,%r5,%r6
	subi,tsv,tr 123,%r5,%r6
	subi,tsv,<> 123,%r5,%r6
	subi,tsv,>= 123,%r5,%r6
	subi,tsv,> 123,%r5,%r6
	subi,tsv,>>= 123,%r5,%r6
	subi,tsv,>> 123,%r5,%r6
	subi,tsv,nsv 123,%r5,%r6
	subi,tsv,ev 123,%r5,%r6
