	.code
	.export		f
f:
	.proc
	.callinfo	frame=0,no_calls
	.entry

	fstws		%fr6R,0(%r26)
	fstws		%fr6L,4(%r26)
	fstws		%fr6,8(%r26)

	fstds		%fr6R,0(%r26)
	fstds		%fr6L,4(%r26)
	fstds		%fr6,8(%r26)

	fldws		0(%r26),%fr6R
	fldws		4(%r26),%fr6L
	fldws		8(%r26),%fr6

	fldds		0(%r26),%fr6R
	fldds		4(%r26),%fr6L
	fldds		8(%r26),%fr6

	fstws		%fr6R,0(%sr0,%r26)
	fstws		%fr6L,4(%sr0,%r26)
	fstws		%fr6,8(%sr0,%r26)

	fstds		%fr6R,0(%sr0,%r26)
	fstds		%fr6L,4(%sr0,%r26)
	fstds		%fr6,8(%sr0,%r26)

	fldws		0(%sr0,%r26),%fr6R
	fldws		4(%sr0,%r26),%fr6L
	fldws		8(%sr0,%r26),%fr6

	fldds		0(%sr0,%r26),%fr6R
	fldds		4(%sr0,%r26),%fr6L
	fldds		8(%sr0,%r26),%fr6

	fstwx		%fr6R,%r25(%r26)
	fstwx		%fr6L,%r25(%r26)
	fstwx		%fr6,%r25(%r26)

	fstdx		%fr6R,%r25(%r26)
	fstdx		%fr6L,%r25(%r26)
	fstdx		%fr6,%r25(%r26)

	fldwx		%r25(%r26),%fr6R
	fldwx		%r25(%r26),%fr6L
	fldwx		%r25(%r26),%fr6

	flddx		%r25(%r26),%fr6R
	flddx		%r25(%r26),%fr6L
	flddx		%r25(%r26),%fr6

	fstwx		%fr6R,%r25(%sr0,%r26)
	fstwx		%fr6L,%r25(%sr0,%r26)
	fstwx		%fr6,%r25(%sr0,%r26)

	fstdx		%fr6R,%r25(%sr0,%r26)
	fstdx		%fr6L,%r25(%sr0,%r26)
	fstdx		%fr6,%r25(%sr0,%r26)

	fldwx		%r25(%sr0,%r26),%fr6R
	fldwx		%r25(%sr0,%r26),%fr6L
	fldwx		%r25(%sr0,%r26),%fr6

	flddx		%r25(%sr0,%r26),%fr6R
	flddx		%r25(%sr0,%r26),%fr6L
	flddx		%r25(%sr0,%r26),%fr6

	bv		%r0(%r2)
	nop

	.exit
	.procend
