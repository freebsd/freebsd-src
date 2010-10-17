; Test that we get an error when directive does not match option.
; Make sure we specify ELF so we don't get spurious failures when testing
; a.out.

; { dg-do assemble }
; { dg-options "--no-underscore --em=criself" }

	.syntax leading_underscore ; { dg-error ".* \.syntax leading_underscore requires .* `--underscore'" }
start:
	nop
