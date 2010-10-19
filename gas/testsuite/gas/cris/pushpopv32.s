; Check that push and pop builtin "macros" aren't recognized for
; v32.
 .text
start:
 subq 4,sp
 move.d r10,[sp]
 subq 4,sp
 move srp,[sp]
 move.d [sp+],r10
 move [sp+],srp
end:
