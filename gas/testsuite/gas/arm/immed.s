@	Tests for complex immediate expressions - none of these need 
@	relocations
	.text
bar:
	mov	r0, #0
	mov	r0, #(. - bar - 8)
	ldr	r0, bar
	ldr	r0, [pc, # (bar - . -8)]
	.space 4096
	mov	r0, #(. - bar - 8) & 0xff
	ldr	r0, [pc, # (bar - . -8) & 0xff]

	@ section padding for a.out's benefit
	nop
	nop
