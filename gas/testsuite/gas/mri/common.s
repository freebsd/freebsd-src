; Test MRI common sections
	common	com1
	ds.l	1
com2	common 00
	ds.l	1
incom	ds.l	1
	common	com1
	ds.l	1
	data
	dc.l	com1
	dc.l	incom
