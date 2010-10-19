; Error cases for invalid operands.
;  { dg-do assemble { target cris-*-* } }
 .text
 .syntax no_register_prefix
start:
 add.w r3,r4,r5 ; { dg-error "(Illegal|Invalid) operands" }
 add.w 42,r4,r5 ; { dg-error "(Illegal|Invalid) operands" }
 add.w [r3],r4,r5 ; Not an error: [r3] implies [r3+0].
 add.w r3,[r3],r4 ; { dg-error "(Illegal|Invalid) operands" }
 add.w r3,[r3] ; { dg-error "(Illegal|Invalid) operands" }
 test.w [r3],r4,r5 ; { dg-error "(Illegal|Invalid) operands" }
 test.d [r3],r4 ; { dg-error "(Illegal|Invalid) operands" }
 move.d [r3],r4,r5 ; { dg-error "(Illegal|Invalid) operands" }

; These two could be seen useful in extreme cases, but those
; would be shadowed by not flagging erroneous use of
; e.g. "test.d $r3" for CRISv32.  If you really need it, use
; e.g. "test.d [$pc+] @ .dword whatever".
 test.d whatever ; { dg-error "(Illegal|Invalid) operands" "" }
 test.d 42 ; { dg-error "(Illegal|Invalid) operands" "" }

 clear.d whatever ; { dg-error "(Illegal|Invalid) operands" }
 clear.d 42 ; { dg-error "(Illegal|Invalid) operands" }
 addi r5,r3 ; { dg-error "(Illegal|Invalid) operands" }

; These two are valid instructions, though not recognized by
; the assembler since they're obscure and generally useless.
 ba [external_symbol] ; { dg-error "(Illegal|Invalid) operands|(B|b)ad expression" }
 ba [r3] ; { dg-error "(Illegal|Invalid) operands|(B|b)ad expression" }
 lsl r3,r5 ; { dg-error "(Illegal|Invalid) operands" }
 xor.d r5,r6 ; { dg-error "(Illegal|Invalid) operands" }

; Addressing modes
 test.d [r3+r4] ; { dg-error "(Illegal|Invalid) operands" }
 test.d [r3=r2+[r4]] ; { dg-error "(Illegal|Invalid) operands" }
 test.d [r3=r2+[r4].w ; { dg-error "(Illegal|Invalid) operands" }
 test.d [r3=r2] ; { dg-error "(Illegal|Invalid) operands" }
 test.d [r3=r2+] ; { dg-error "(Illegal|Invalid) operands|(B|b)ad expression" }
 test.d [r3++] ; { dg-error "(Illegal|Invalid) operands|(B|b)ad expression" }

; I think these should be valid; a dip with "postincrement" on
; the insn that follows.
 test.d [r3=external_symbol] ; { dg-error "(Illegal|Invalid) operands" }
 test.d [r3=[r4]] ; { dg-error "(Illegal|Invalid) operands" }
 test.d [r3=[r4+]] ; { dg-error "(Illegal|Invalid) operands" }

 test.d [[r3+r4.b]] ; { dg-error "(Illegal|Invalid) operands" }
 test.d [r3=external+[r5]] ; { dg-error "(Illegal|Invalid) operands" }
 test.d [r3=[r5]+external] ; { dg-error "(Illegal|Invalid) operands" }
 test.d [r3+[r3+r5.d]] ; { dg-error "(Illegal|Invalid) operands" }
 test.d [r3+[r3+external]] ; { dg-error "(Illegal|Invalid) operands" }
