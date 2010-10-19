; { dg-do assemble { target cris-*-* } }
; { dg-options "--march=v32" }
x:
 ; There are no "push" or "pop" synonyms for v32.
 push $r10	; { dg-error "Unknown" }
 push $srp	; { dg-error "Unknown" }
 pop $r8	; { dg-error "Unknown" }
 pop $mof	; { dg-error "Unknown" }
