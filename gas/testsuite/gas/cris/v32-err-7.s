; Error for lapcq out-of-range.
; { dg-do assemble }
; { dg-options "--march=v32 --em=criself" }

a:
 nop
 lapcq a,$r10	; { dg-error "not in 4.bit unsigned range" }
 lapcq x,$r11	; { dg-error "not in 4.bit unsigned range" }
 .space 30
x:
