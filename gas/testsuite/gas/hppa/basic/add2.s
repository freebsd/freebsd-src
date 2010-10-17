	.LEVEL 2.0
	.code
	.align 4
; Basic add/sh?add instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	add,*  %r4,%r5,%r6
	add,*=  %r4,%r5,%r6
	add,*<  %r4,%r5,%r6
	add,*<=  %r4,%r5,%r6
	add,*nuv  %r4,%r5,%r6
	add,*znv  %r4,%r5,%r6
	add,*sv  %r4,%r5,%r6
	add,*od  %r4,%r5,%r6
	add,*tr  %r4,%r5,%r6
	add,*<>  %r4,%r5,%r6
	add,*>=  %r4,%r5,%r6
	add,*>  %r4,%r5,%r6
	add,*uv  %r4,%r5,%r6
	add,*vnz  %r4,%r5,%r6
	add,*nsv  %r4,%r5,%r6
	add,*ev  %r4,%r5,%r6

	add,l,*  %r4,%r5,%r6
	add,l,*=  %r4,%r5,%r6
	add,l,*<  %r4,%r5,%r6
	add,l,*<=  %r4,%r5,%r6
	add,l,*nuv  %r4,%r5,%r6
	add,l,*znv  %r4,%r5,%r6
	add,l,*sv  %r4,%r5,%r6
	add,l,*od  %r4,%r5,%r6
	add,l,*tr  %r4,%r5,%r6
	add,l,*<>  %r4,%r5,%r6
	add,l,*>=  %r4,%r5,%r6
	add,l,*>  %r4,%r5,%r6
	add,l,*uv  %r4,%r5,%r6
	add,l,*vnz  %r4,%r5,%r6
	add,l,*nsv  %r4,%r5,%r6
	add,l,*ev  %r4,%r5,%r6

	add,tsv,*  %r4,%r5,%r6
	add,tsv,*=  %r4,%r5,%r6
	add,tsv,*<  %r4,%r5,%r6
	add,tsv,*<=  %r4,%r5,%r6
	add,tsv,*nuv  %r4,%r5,%r6
	add,tsv,*znv  %r4,%r5,%r6
	add,tsv,*sv  %r4,%r5,%r6
	add,tsv,*od  %r4,%r5,%r6
	add,tsv,*tr  %r4,%r5,%r6
	add,tsv,*<>  %r4,%r5,%r6
	add,tsv,*>=  %r4,%r5,%r6
	add,tsv,*>  %r4,%r5,%r6
	add,tsv,*uv  %r4,%r5,%r6
	add,tsv,*vnz  %r4,%r5,%r6
	add,tsv,*nsv  %r4,%r5,%r6
	add,tsv,*ev  %r4,%r5,%r6

	add,dc,*  %r4,%r5,%r6
	add,dc,*=  %r4,%r5,%r6
	add,dc,*<  %r4,%r5,%r6
	add,dc,*<=  %r4,%r5,%r6
	add,dc,*nuv  %r4,%r5,%r6
	add,dc,*znv  %r4,%r5,%r6
	add,dc,*sv  %r4,%r5,%r6
	add,dc,*od  %r4,%r5,%r6
	add,dc,*tr  %r4,%r5,%r6
	add,dc,*<>  %r4,%r5,%r6
	add,dc,*>=  %r4,%r5,%r6
	add,dc,*>  %r4,%r5,%r6
	add,dc,*uv  %r4,%r5,%r6
	add,dc,*vnz  %r4,%r5,%r6
	add,dc,*nsv  %r4,%r5,%r6
	add,dc,*ev  %r4,%r5,%r6

	add,dc,tsv,*  %r4,%r5,%r6
	add,dc,tsv,*=  %r4,%r5,%r6
	add,dc,tsv,*<  %r4,%r5,%r6
	add,dc,tsv,*<=  %r4,%r5,%r6
	add,dc,tsv,*nuv  %r4,%r5,%r6
	add,dc,tsv,*znv  %r4,%r5,%r6
	add,dc,tsv,*sv  %r4,%r5,%r6
	add,dc,tsv,*od  %r4,%r5,%r6
	add,tsv,dc,*tr  %r4,%r5,%r6
	add,tsv,dc,*<>  %r4,%r5,%r6
	add,tsv,dc,*>=  %r4,%r5,%r6
	add,tsv,dc,*>  %r4,%r5,%r6
	add,tsv,dc,*uv  %r4,%r5,%r6
	add,tsv,dc,*vnz  %r4,%r5,%r6
	add,tsv,dc,*nsv  %r4,%r5,%r6
	add,tsv,dc,*ev  %r4,%r5,%r6
