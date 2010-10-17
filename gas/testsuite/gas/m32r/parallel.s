; Test error messages where parallel instructions conflict

; { dg-options "-m32rx" }
; { dg-do assemble { target m32r-*-* } }

	.text
	.global parallel
parallel:
	mv r1,r0 || mv r2,r1
	; { dg-warning "output of 1st instruction" "parallel output overlaps input" { target *-*-* } { 9 } }
	mv r1,r0 || mv r0,r2
	; { dg-warning "output of 2nd instruction" "parallel output overlaps input" { target *-*-* } { 11 } }
	mv r1,r0 || mv r1,r2
	; { dg-error "instructions write to the same destination register" "parallel overlapping destinations" { target *-*-* } { 13 } }
