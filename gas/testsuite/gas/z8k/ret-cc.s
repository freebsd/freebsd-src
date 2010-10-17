	.text

	ret f
	ret lt
	ret le
	ret ule
	ret ov
	ret pe
	ret mi
	ret eq
	ret z
	ret c
	ret ult
	ret t
	ret ge
	ret gt
	ret ugt
	ret nov
	ret NOV 
	ret po
	ret pl
	ret ne
	ret nz
	ret nc   ! ssss
	ret uge    ! dddd
	ret	ov/pe
	ret	c/ult
	ret	nov/po
	ret	nc/uge
	ret 
	ret
dd:
	jr	t,dd
	jr	dd


	nop 
	nop

