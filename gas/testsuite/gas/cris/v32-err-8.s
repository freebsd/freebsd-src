; { dg-do assemble }
; { dg-options "--march=common_v10_v32" }

; USP does not have the same register number in v10 as in v32.

 move $r10,$usp			; { dg-error "(Illegal|Invalid) operands" }
 move 0xfabb0,$usp		; { dg-error "(Illegal|Invalid) operands" }
 move $usp,[$r5]		; { dg-error "(Illegal|Invalid) operands" }
 move [$r12],$usp		; { dg-error "(Illegal|Invalid) operands" }
