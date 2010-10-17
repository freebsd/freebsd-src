	.text
	.global _start
_start:
        mov.w   r0,r0
        beq     .L1
        jsr     @_bar
.L1:
        rts
_bar:
        rts
