
; { dg-do assemble { target m32r-*-* } }

	.text
	nop
	nop
bar:
	.section .text2
	.2byte bar - .  ; { dg-error "can't export reloc type 11" } 
	.byte bar - .  ; { dg-error "can\'t export reloc type 7" } 
