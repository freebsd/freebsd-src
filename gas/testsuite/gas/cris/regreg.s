; @OC@ test
; Template for testing register-to-register instructions.

; Note that sizes have to be tested by iterating over them; they
; are not included here in order to cover fixed-size instructions
; too.  This may seem wasteful of machine time, but then that time
; is less expensive than any other time and still falling in cost.

 .text
 .syntax no_register_prefix
start:
 @OC@	r1,r3
 @OC@	r0,r0
 @OC@	r0,r13
 @OC@	r5,r0
 @OC@	r13,r13
 @OC@	r9,r3
end:
