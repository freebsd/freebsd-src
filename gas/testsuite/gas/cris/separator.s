; Testing separators.  This file should only have nops.  See
; seperror for constructs that are syntax errors.
;
 .text
start:
; Note that the next line is a syntax error.  Should it be?
; nop # moveq 0,r10 -- a comment, not command separator
; The next line is *not* a syntax error.  Should it?
 nop; moveq 1,r10
; The next line is a syntax error.  Maybe it shouldn't.
; nop # moveq 2,r10 -- a comment, not command separator
 nop ; moveq 3,r10
; moveq 4,r10
# moveq 5,r10
# 123 456 ; not a syntax error, not a line directive.
end:
