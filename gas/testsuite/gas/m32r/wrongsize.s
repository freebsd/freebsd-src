; Test error messages in instances where an insn of a particular size
; is required.

; { dg-do assemble { target m32r-*-* } }

wrongsize:
	cmpi r8,#10 -> ldi r0,#8  ; { dg-error "not a 16 bit instruction" }
	ldi r0,#8 -> cmpi r8,#10  ; { dg-error "not a 16 bit instruction" }
	cmpi r8,#10 || ldi r0,#8  ; { dg-error "not a 16 bit instruction" }
	ldi r0,#8 || cmpi r8,#10  ; { dg-error "not a 16 bit instruction" }
