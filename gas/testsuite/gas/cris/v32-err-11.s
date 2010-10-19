; { dg-do assemble }
; { dg-options "--march=v32" }

; Check that explicit contants out-of-range for addoq are
; identified.  See also v32-err-10.s.

 .text
here:
 addoq 133,$r0,$acr			; { dg-error "not in 8 bit signed range" }
 addoq 128,$r0,$acr			; { dg-error "not in 8 bit signed range" }
 addoq -129,$r0,$acr			; { dg-error "not in 8 bit signed range" }
 addoq 127,$r0,$acr
 addoq -128,$r0,$acr
