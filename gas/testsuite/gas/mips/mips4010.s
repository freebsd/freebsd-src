	.text

stuff:
	.ent stuff
	flushi
	flushd
	flushid
	madd $4,$5
	maddu $5,$6
	ffc $6,$7
	ffs $7,$8
	msub $8,$9
	msubu $9,$10
	selsl $10,$11,$12
	selsr $11,$12,$13
	waiti
	wb 16($14)
	addciu $14,$15,16
	nop
	nop
	.end stuff
