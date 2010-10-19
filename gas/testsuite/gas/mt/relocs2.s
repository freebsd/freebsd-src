	.text
	;; Put code near the top of the address space
text:
	.global i2
i2:	
	
	add R5, R6, R7

	.data
	;; Note that the .org that follows is more or less equivalent
	;; to a .space, since the amount specified will be treated like
	;; padding to be added between the .data section in relocs1.s
	;; and this one.
	;; Note also that the two test variables (d2 & d3) are intentionally
	;; roughly $100 apart, so that the FR9 relocation processing in
	;; bfd/elf32-ip2k.c (ip2k_final_link_relocate) is tested a little more.
	.org $e0
	.global d2
d2:	.byte 2
	.space $100
	.global d3
d3:	.byte 3
