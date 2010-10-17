; Test line continuation, making sure a commented line is not
; broken up and that a continued line is.
 .text
 .syntax no_register_prefix
start:
; 	move.d r1,[r8]@ move.d r2,[r8]
 	move.d r7,[r8]@ move.d r9,[r8]
