	.globl text_symbol
	.text
text_symbol:	
	.long	1
	.section .post_text_reserve,"w", %nobits
	.space 160
