; Test that we get errors when we require a register prefix.

; { dg-do assemble }

	.syntax register_prefix
start:

; Some simple tests that we indeed require a register prefix, and some
; that should not be flagged as syntax errors.

	push srp		; { dg-error "(Illegal|Invalid) operands" }
	push r3			; { dg-error "(Illegal|Invalid) operands" }
	move.d $r7,r8		; { dg-error "(Illegal|Invalid) operands" }
	move.d r8,[$r11]	; { dg-error "(Illegal|Invalid) operands" }
	move.d $r8,[$r11+]
	move.d $r8,[$r10+$r9.b]
	move.d $r7,[$r10+[$r1].d]
	move.d $r7,[$r10+[$r3+].w]
	move $r8,srp		; { dg-error "(Illegal|Invalid) operands" }
	move ccr,$r13		; { dg-error "(Illegal|Invalid) operands" }
	movem r4,[$r12+]	; { dg-error "(Illegal|Invalid) operands" }

; Here we have no ambiguity; r10 can only be a symbol when we reuire a
; prefix.  It does not just miss a size specifier, e.g. as in [r12+r10.d].
	move.d $r13,[$r12+r10]

	.syntax no_register_prefix

; Perhaps in this one we should backtrack and retry r10 as a symbol, but
; the ambiguity is closer to a programming error, so we should catch it as
; such.
	move.d $r13,[$r12+r10]	; { dg-error "(Illegal|Invalid) operands" }
	move.d r13,[r12+r16]	; No register named r16 so must be a symbol.
	nop
