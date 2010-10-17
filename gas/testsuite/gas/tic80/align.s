;;	Test the .align directive.

	.text
	
	;; This should generate 0xAB000000
	.byte	0xAB
	.align			; Should default to 4 byte alignment

	;; This should generate 0xCD00EF00
	.byte	0xCD
	.align	2		;  Should align to the next 2-byte boundary (pad with one null byte)
	.byte	0xEF
	.align	1

	;; This should generate 0xF1000000
	.align	4		;  Should not affect alignment (already on 4)
	.byte	0xF1
	.align	4		;  Should align to next 4 byte boundary

	;; This should generate 0xEE000000 since we are already on 4 byte alignment
	.byte 0xEE
	.align	8

	;; This should generate 0xAC000000 0x00000000
	.byte	0xAC
	.align	8
	
	;; This should generate 0xAB000000 0x00000000 since we are at 8 byte alignment
	.byte	0xAB
	.align	16

	;; This should generate 0xFE000000 0x00000000 0x00000000 0x00000000
	.byte	0xFE
	.align	16
	
	;; This just forces the disassembler to not print ... for trailing nulls
	.byte 0xDE, 0xAD, 0xBE, 0xEF
