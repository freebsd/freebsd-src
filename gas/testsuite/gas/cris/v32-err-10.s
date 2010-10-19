; { dg-do assemble }
; { dg-options "--march=v32" }

; Check that explicit contants out-of-range for addo are
; identified.  We don't check addoq here, since that range check
; is done at a later stage which isn't entered if there were
; errors.

 .text
here:
 addo.b 133,$r0,$acr			; { dg-error "not in 8 bit signed range" }
 addo.b 128,$r0,$acr			; { dg-error "not in 8 bit signed range" }
 addo.b -129,$r0,$acr			; { dg-error "not in 8 bit signed range" }
 addo.b 127,$r0,$acr
 addo.b -128,$r0,$acr
 addo.w 32768,$r0,$acr			; { dg-error "not in 16 bit signed range" }
 addo.w -32769,$r0,$acr			; { dg-error "not in 16 bit signed range" }
 addo.w 32767,$r0,$acr
 addo.w -32768,$r0,$acr
