; { dg-do assemble }
; { dg-options " --underscore --march=common_v10_v32 --em=criself" }
; { dg-error ".word offset handling is not implemented" "err for broken .word" { target cris-*-* } 0 }

; Tests that broken words don't crash, just give a message when
; in compatibility mode.

sym2:	moveq 0,r0
	.word	sym1 - sym2
	moveq 1,r0
	moveq 2,r0
	.space	32766, 0
sym1:	moveq 3,r0

