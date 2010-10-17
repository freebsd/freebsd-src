	.code
	.align 4

  	ldil	L%0xc0001004,%r1
  	ble	R%0xc0001004(%sr7,%r1)
