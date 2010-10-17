; Test error cases for separators.
; This one should not treat a ";" as a line separator, not even
; just after an instruction.
 .text
 .syntax no_register_prefix
start:
 moveq 0,r2;nop
 di
