; Test (no_)register_prefixes a bit.  Register prefix may or may not be
; mandated when we get here.

start:
; Ambiguous.  Depends on default.
	move.d r5,$r5
	move r4,$ibr
	move.d $r4,[r10+1]
	jsr r10
	move.d [r0],$r7

; Non-ambiguous, with a prefix.

	push $srp
	move $irp,$r4
	move.d $r4,[$r0+$r10.b]
	move $ccr,[$pc+r16]

	.syntax no_register_prefix

; Some invalid with mandated register prefix; check that they pass.

	push srp
	move.d r4,[r0+r10.d]
	move $ccr,[$pc+r16]

; Ambiguity interpreted one way...

	move.d r5,$r5
	move r4,$ibr
	move.d $r4,[r10+1]
	jsr r10

	.syntax register_prefix

; Ambiguity interpreted the other way.

	move.d r5,$r5
	move r4,$ibr
	move.d $r4,[r10+1]
	jsr r10
