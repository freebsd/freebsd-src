	.code

	.align 4
; A comment.  This should not be interpreted as a label, but both of the
; following statements should.
label_without_colon
label_with_colon:

; A problem tege found...
; Input scrubbing in gas makes life a real nightmare for assemblers
; in which the *position* within a line determines how to interpret
; a stream a characters.  These test one particular case where gas
; had the tendency to delete the whitespace between the opcode and
; operands if a label without a colon began a line, and the operands
; started with a non-numeric character.
L$1	add %r2,%r2,%r2
L$2:	add %r2,%r2,%r2
L$3
	add %r2,%r2,%r2

L$4	add %r2,%r2,%r2
L$5:	add %r2,%r2,%r2
L$6
	add %r2,%r2,%r2

; An instruction or pseudo-op may begin anywhere after column 0.
 b,n label_without_colon
