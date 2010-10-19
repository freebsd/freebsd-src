; { dg-do assemble }
; { dg-options "--march=v32" }

; "Test.m R" doesn't exist.

 test.d $r10			; { dg-error "(Illegal|Invalid) operands" }
 test.w $r0			; { dg-error "(Illegal|Invalid) operands" }
 test.b $acr			; { dg-error "(Illegal|Invalid) operands" }
