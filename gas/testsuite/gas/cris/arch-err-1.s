; Test mismatch of --march=ARCH1 and .arch ARCH2.
; { dg-do assemble }
 .arch something ; { dg-error "unknown operand to .arch" }

