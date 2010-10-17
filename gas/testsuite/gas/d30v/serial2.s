# D30V serial execution test
	
	.text
	
	bra -3 -> add r3,r0,0	; Invalid
	bsr -3 -> add r3,r0,0	; Invalid

	bra/tx -3 -> add r3,r0,0 ;       Valid
	bsr/tx -3 -> add r3,r0,0 ;       Valid

	bsr -3 -> bsr -10	;       Invalid
	bsr -3 -> bsr/xt -10    ;       Invalid
	bsr/tx -3 -> bsr -10    ;       Valid
	bsr/tx -3 -> bsr/fx -10 ;       Valid

	bra -3 -> bra  10       ;      Invalid
	bra -3 -> bra/tx 10     ;      Invalid
	bra/tx -3 -> bra 10     ;      Valid
	bra/tx -3 -> bra/fx 10  ;      Valid

	bsr -3 -> bra 10        ;      Invalid
	bsr -3 -> bra/tx 10     ;      Invalid
	bsr/tx -3 -> bra 10     ;      Valid
	bsr/tx -3 -> bra/fx 10  ;      Valid

	bra -3 -> bsr 10        ;      Invalid
	bra -3 -> bsr/tx 10     ;      Invalid
	bra/tx -3 -> bsr 10     ;      Valid
	bra/tx -3 -> bsr/fx 10  ;      Valid
