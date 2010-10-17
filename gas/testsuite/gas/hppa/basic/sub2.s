	.LEVEL 2.0
	.code
	.align 4
; Basic immediate instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	sub,* %r4,%r5,%r6
	sub,*= %r4,%r5,%r6
	sub,*< %r4,%r5,%r6
	sub,*<= %r4,%r5,%r6
	sub,*<< %r4,%r5,%r6
	sub,*<<= %r4,%r5,%r6
	sub,*sv %r4,%r5,%r6
	sub,*od %r4,%r5,%r6
	sub,*tr %r4,%r5,%r6
	sub,*<> %r4,%r5,%r6
	sub,*>= %r4,%r5,%r6
	sub,*> %r4,%r5,%r6
	sub,*>>= %r4,%r5,%r6
	sub,*>> %r4,%r5,%r6
	sub,*nsv %r4,%r5,%r6
	sub,*ev %r4,%r5,%r6

	sub,tsv,* %r4,%r5,%r6
	sub,tsv,*= %r4,%r5,%r6
	sub,tsv,*< %r4,%r5,%r6
	sub,tsv,*<= %r4,%r5,%r6
	sub,tsv,*<< %r4,%r5,%r6
	sub,tsv,*<<= %r4,%r5,%r6
	sub,tsv,*sv %r4,%r5,%r6
	sub,tsv,*od %r4,%r5,%r6
	sub,tsv,*tr %r4,%r5,%r6
	sub,tsv,*<> %r4,%r5,%r6
	sub,tsv,*>= %r4,%r5,%r6
	sub,tsv,*> %r4,%r5,%r6
	sub,tsv,*>>= %r4,%r5,%r6
	sub,tsv,*>> %r4,%r5,%r6
	sub,tsv,*nsv %r4,%r5,%r6
	sub,tsv,*ev %r4,%r5,%r6

	sub,db,* %r4,%r5,%r6
	sub,db,*= %r4,%r5,%r6
	sub,db,*< %r4,%r5,%r6
	sub,db,*<= %r4,%r5,%r6
	sub,db,*<< %r4,%r5,%r6
	sub,db,*<<= %r4,%r5,%r6
	sub,db,*sv %r4,%r5,%r6
	sub,db,*od %r4,%r5,%r6
	sub,db,*tr %r4,%r5,%r6
	sub,db,*<> %r4,%r5,%r6
	sub,db,*>= %r4,%r5,%r6
	sub,db,*> %r4,%r5,%r6
	sub,db,*>>= %r4,%r5,%r6
	sub,db,*>> %r4,%r5,%r6
	sub,db,*nsv %r4,%r5,%r6
	sub,db,*ev %r4,%r5,%r6

	sub,db,tsv,* %r4,%r5,%r6
	sub,db,tsv,*= %r4,%r5,%r6
	sub,db,tsv,*< %r4,%r5,%r6
	sub,db,tsv,*<= %r4,%r5,%r6
	sub,db,tsv,*<< %r4,%r5,%r6
	sub,db,tsv,*<<= %r4,%r5,%r6
	sub,db,tsv,*sv %r4,%r5,%r6
	sub,db,tsv,*od %r4,%r5,%r6
	sub,tsv,db,*tr %r4,%r5,%r6
	sub,tsv,db,*<> %r4,%r5,%r6
	sub,tsv,db,*>= %r4,%r5,%r6
	sub,tsv,db,*> %r4,%r5,%r6
	sub,tsv,db,*>>= %r4,%r5,%r6
	sub,tsv,db,*>> %r4,%r5,%r6
	sub,tsv,db,*nsv %r4,%r5,%r6
	sub,tsv,db,*ev %r4,%r5,%r6

	sub,tc,* %r4,%r5,%r6
	sub,tc,*= %r4,%r5,%r6
	sub,tc,*< %r4,%r5,%r6
	sub,tc,*<= %r4,%r5,%r6
	sub,tc,*<< %r4,%r5,%r6
	sub,tc,*<<= %r4,%r5,%r6
	sub,tc,*sv %r4,%r5,%r6
	sub,tc,*od %r4,%r5,%r6
	sub,tc,*tr %r4,%r5,%r6
	sub,tc,*<> %r4,%r5,%r6
	sub,tc,*>= %r4,%r5,%r6
	sub,tc,*> %r4,%r5,%r6
	sub,tc,*>>= %r4,%r5,%r6
	sub,tc,*>> %r4,%r5,%r6
	sub,tc,*nsv %r4,%r5,%r6
	sub,tc,*ev %r4,%r5,%r6

	sub,tc,tsv,* %r4,%r5,%r6
	sub,tc,tsv,*= %r4,%r5,%r6
	sub,tc,tsv,*< %r4,%r5,%r6
	sub,tc,tsv,*<= %r4,%r5,%r6
	sub,tc,tsv,*<< %r4,%r5,%r6
	sub,tc,tsv,*<<= %r4,%r5,%r6
	sub,tc,tsv,*sv %r4,%r5,%r6
	sub,tc,tsv,*od %r4,%r5,%r6
	sub,tsv,tc,*tr %r4,%r5,%r6
	sub,tsv,tc,*<> %r4,%r5,%r6
	sub,tsv,tc,*>= %r4,%r5,%r6
	sub,tsv,tc,*> %r4,%r5,%r6
	sub,tsv,tc,*>>= %r4,%r5,%r6
	sub,tsv,tc,*>> %r4,%r5,%r6
	sub,tsv,tc,*nsv %r4,%r5,%r6
	sub,tsv,tc,*ev %r4,%r5,%r6
