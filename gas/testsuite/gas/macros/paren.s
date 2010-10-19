 .data
 .macro m x
 .byte (\x)
 .endm

	m	(1)
	m	(!1)
	m	(1)+(1)
	m	1+(1)
	m	(1 + 1)
	m	(1 + 1)*(1 + 1)
	m	(! 0)+(! 0)
