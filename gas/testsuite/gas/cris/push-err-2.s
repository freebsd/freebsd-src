; { dg-do assemble { target cris-*-* } }
; { dg-options "--march=common_v10_v32" }
x:
; There are no "push" or "pop" synonyms for the compatible
; subset of v10 and v32.
 push $r10	; { dg-error "Unknown" }
 push $srp	; { dg-error "Unknown" }
 pop $r8	; { dg-error "Unknown" }
 pop $mof	; { dg-error "Unknown" }
