; Test that an "@" does do TRT in a macro, and does not break up
; lines.
	.syntax no_register_prefix
	.macro test_h_gr val reg
	cmp.d \val,\reg
	beq test_gr\@
	nop
test_gr\@:
	.endm

start:
        test_h_gr 5,r0
        nop
