; Error for flags not applicable to current arch.
; #2: Error for v32 flags for pre-v32.
; { dg-do assemble }
; { dg-options "--march=v0_v10" }

y:
 clearf p	; { dg-error "(Illegal|Invalid) operands" }
 setf P		; { dg-error "(Illegal|Invalid) operands" }
 setf u		; { dg-error "(Illegal|Invalid) operands" }
 clearf U	; { dg-error "(Illegal|Invalid) operands" }
 clearf d
 setf D
 clearf e
 setf E
 clearf b
 setf B
 setf m
 clearf M
