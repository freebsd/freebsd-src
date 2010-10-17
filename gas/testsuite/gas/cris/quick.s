; @OC@ test
; Template for generic "quick" operand checking.

; To see that expressions with symbols are evaluated correctly.
 .set twenty2, 22
 .set mtwenty2, -22

 .text
 .syntax no_register_prefix
start:

;;;;;;;;;;;;;;;;;
;
; Unsigned 5 bits.

 @OC@	twenty2,r3
 @OC@	-twenty2+33,r5
 @OC@	twenty2-22,r10
 @OC@	31,r5
 @OC@	1,r4
 @OC@	0,r11
 @OC@	32-twenty2,r11
 @OC@	-0,r12

unsigned6:		; u6
;;;;;;;;;;;;;;;;;
;
; Unsigned 6 bits

 @OC@	twenty2*2,r3		; u6
 @OC@	-twenty2+33*2+13,r5	; u6
 @OC@	twenty2-22,r10		; u6
 @OC@	31*2,r5			; u6
 @OC@	twenty2*3-3,r4		; u6
 @OC@	twenty2*3-4,r5		; u6
 @OC@	63,r11			; u6
 @OC@	32,r11			; u6

signed6:		; s6
;;;;;;;;;;;;;;;;;
;
; Signed 6 bits.
; Only need to check negative operands here; the unsigned 5
; bits cases above covers positive numbers.
 @OC@	-31,r3			; s6
 @OC@	mtwenty2,r3		; s6
 @OC@	mtwenty2*2+14,r3	; s6
 @OC@	-64+35,r7		; s6
 @OC@	-1,r13			; s6
 @OC@	-twenty2+21,r12		; s6
end:
