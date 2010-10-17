	.code
	.align 4
; Basic immediate instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	fldwx %r4(%sr0,%r5),%fr6
	fldwx,s %r4(%sr0,%r5),%fr6
	fldwx,m %r4(%sr0,%r5),%fr6
	fldwx,sm %r4(%sr0,%r5),%fr6
	flddx %r4(%sr0,%r5),%fr6
	flddx,s %r4(%sr0,%r5),%fr6
	flddx,m %r4(%sr0,%r5),%fr6
	flddx,sm %r4(%sr0,%r5),%fr6
	fstwx %fr6,%r4(%sr0,%r5)
	fstwx,s %fr6,%r4(%sr0,%r5)
	fstwx,m %fr6,%r4(%sr0,%r5)
	fstwx,sm %fr6,%r4(%sr0,%r5)
	fstdx %fr6,%r4(%sr0,%r5)
	fstdx,s %fr6,%r4(%sr0,%r5)
	fstdx,m %fr6,%r4(%sr0,%r5)
	fstdx,sm %fr6,%r4(%sr0,%r5)
	fstqx %fr6,%r4(%sr0,%r5)
	fstqx,s %fr6,%r4(%sr0,%r5)
	fstqx,m %fr6,%r4(%sr0,%r5)
	fstqx,sm %fr6,%r4(%sr0,%r5)

	fldws 0(%sr0,%r5),%fr6
	fldws,mb 0(%sr0,%r5),%fr6
	fldws,ma 0(%sr0,%r5),%fr6
	fldds 0(%sr0,%r5),%fr6
	fldds,mb 0(%sr0,%r5),%fr6
	fldds,ma 0(%sr0,%r5),%fr6
	fstws %fr6,0(%sr0,%r5)
	fstws,mb %fr6,0(%sr0,%r5)
	fstws,ma %fr6,0(%sr0,%r5)
	fstds %fr6,0(%sr0,%r5)
	fstds,mb %fr6,0(%sr0,%r5)
	fstds,ma %fr6,0(%sr0,%r5)
	fstqs %fr6,0(%sr0,%r5)
	fstqs,mb %fr6,0(%sr0,%r5)
	fstqs,ma %fr6,0(%sr0,%r5)
