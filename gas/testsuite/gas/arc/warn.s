; Test ARC specific assembler warnings
;
; { dg-do assemble { target arc-*-* } }

	b.d foo
	mov r0,256	; { dg-warning "8 byte instruction in delay slot" "8 byte instruction in delay slot" }

	j.d foo		; { dg-warning "8 byte jump instruction with delay slot" "8 byte jump instruction with delay slot" }
	mov r0,r1

foo:
