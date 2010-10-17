* symbol .set value
* symbol .equ value
* These two are completely interchangeable
	.global AUX_R1, INDEX, LABEL, SYMTAB, NSYMS
AUX_R1	.set	AR1			
	STM	#56h, AUX_R1		
INDEX	.equ	100/2 +3		
	ADD	#INDEX,A		
LABEL	.word	10			
SYMTAB	.set	LABEL + 1
NSYMS	.set	INDEX
	.word	NSYMS			
	.end
