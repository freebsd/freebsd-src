; Test that we get an error with mismatching options.

; { dg-do assemble }
; { dg-options "--no-underscore --em=crisaout" }
; { dg-error ".* --no-underscore is invalid with a.out format" "" { target cris-*-* } 0 }

start:
	nop
