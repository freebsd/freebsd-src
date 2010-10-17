; Test error messages in instances where output operands interfere.

; { dg-do assemble { target m32r-*-* } }
; { dg-options -m32rx }

interfere:
	trap #1      || cmp  r3, r4	; { dg-error "write to the same" }
		; { dg-warning "same" "out->in" { target *-*-* } { 7 } }
	rte          || addx r3, r4	; { dg-error "write to the same" }
		; { dg-warning "same" "out->in" { target *-*-* } { 9 } }
	cmp  r1, r2  || addx r3, r4	; { dg-error "write to the same" }
		; { dg-warning "same" "out->in" { target *-*-* } { 11 } }
	mvtc r0, psw || addx r1, r4	; { dg-error "write to the same" }
		; { dg-warning "same" "out->in" { target *-*-* } { 13 } }
