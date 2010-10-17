; Check if labels are mistaken for floats.
; Since we don't handle floats at all, "0f" should not be mistaken for a
; floating-point number at any time.
 .text
 .syntax no_register_prefix
start:
 move.d 0f,r4
0:
 cmp.d 0b,r4
