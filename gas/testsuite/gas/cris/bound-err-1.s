; { dg-do assemble { target cris-*-* } }
; { dg-options "--march=v32" }
x:
 ; Memory operand for bound didn't make it to v32.  Check that
 ; it's flagged as an error.
 bound.b [r3],r7	; { dg-error "operands" }
 bound.w [r8+],r1	; { dg-error "operands" }
 bound.d [r11],r3	; { dg-error "operands" }
 nop ; For alignment purposes.
