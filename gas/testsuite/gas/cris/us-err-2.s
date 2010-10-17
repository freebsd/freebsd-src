; Test that we get an error when directive does not match option.
; Make sure we specify ELF so we don't get spurious failures when testing
; a.out.

; { dg-do assemble }
; { dg-options "--underscore" }

	.syntax no_leading_underscore ; { dg-error ".* \.syntax no_leading_underscore requires .* `--no-underscore'" }
start:
	nop
