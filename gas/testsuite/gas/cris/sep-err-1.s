; Test error cases for separators.
;  { dg-do assemble { target cris-*-* } }
 .text
 .syntax no_register_prefix
start:
 nop|moveq 0,r10 ; { dg-error "Unknown opcode" }
