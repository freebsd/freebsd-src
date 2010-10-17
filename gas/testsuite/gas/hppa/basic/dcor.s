	.level 1.1
	.code
	.align 4
; decimal correction instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	dcor %r4,%r5
	dcor,sbz %r4,%r5
	dcor,shz %r4,%r5
	dcor,sdc %r4,%r5
	dcor,sbc %r4,%r5
	dcor,shc %r4,%r5
	dcor,tr %r4,%r5
	dcor,nbz %r4,%r5
	dcor,nhz %r4,%r5
	dcor,ndc %r4,%r5
	dcor,nbc %r4,%r5
	dcor,nhc %r4,%r5

	idcor %r4,%r5
	idcor,sbz %r4,%r5
	idcor,shz %r4,%r5
	idcor,sdc %r4,%r5
	idcor,sbc %r4,%r5
	idcor,shc %r4,%r5
	idcor,tr %r4,%r5
	idcor,nbz %r4,%r5
	idcor,nhz %r4,%r5
	idcor,ndc %r4,%r5
	idcor,nbc %r4,%r5
	idcor,nhc %r4,%r5

	dcor,i %r4,%r5
	dcor,i,sbz %r4,%r5
	dcor,i,shz %r4,%r5
	dcor,i,sdc %r4,%r5
	dcor,i,sbc %r4,%r5
	dcor,i,shc %r4,%r5
	dcor,i,tr %r4,%r5
	dcor,i,nbz %r4,%r5
	dcor,i,nhz %r4,%r5
	dcor,i,ndc %r4,%r5
	dcor,i,nbc %r4,%r5
	dcor,i,nhc %r4,%r5

