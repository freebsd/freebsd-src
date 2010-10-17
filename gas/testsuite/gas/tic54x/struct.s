* .struct
* .union
* .tag		
REAL_REC .struct			; stag = REAL_REC
NUM	.int				; NUM = 0
DEN	.int				; DEN = 1
REAL_LEN .endstruct			; REAL_LEN = 2
	
	add	REAL + REAL_REC.DEN, a	; 000000 0001
	.bss	REAL, REAL_LEN		; 000000 0000 (len = 2)
	
CPLX_REC .struct
REALI	.tag REAL_REC
IMAGI	.tag REAL_REC
CPLX_LEN .endstruct				
	; apply the CPLX_REC structure format to .bss var COMPLEX
	
	.bss	COMPLEX, CPLX_LEN	; 000002 0000 (len = 4)
COMPLEX .tag CPLX_REC
	add	COMPLEX.REALI, a	; 000001 0002
	stl	a, COMPLEX.REALI	; 000002 8002
	add	COMPLEX.IMAGI, b	; 000003 0104

	; anonymous struct; symbols become global
	.struct
X	.int
Y	.int
Z	.int
	.endstruct 		
	
BIT_REC	.struct
STREAM	.string	64			;
BIT7	.field	7			; bit7 = 64
BIT9	.field	9			; bit9 = 64
BIT10	.field	10			; bit10 = 65
X_INT	.int				; x_int = 66
BIT_LEN .endstruct			; bit_len = 67
	
	.bss	BITS, BIT_LEN		; 000006 0000 (len = 67)
BITS	.tag	BIT_REC	
	add	BITS.BIT7,a		; 000004 0046
	and	#007Fh, a		; 000005 f030
					; 000006 007f
	.end
