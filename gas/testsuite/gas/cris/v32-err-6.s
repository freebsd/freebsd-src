; Error for flags not applicable to current arch.
; #4: Error for v32 and pre-v10 flags for v10.
; { dg-do assemble }
; { dg-options "--march=v10" }

y:
 clearf p	; { dg-error "(Illegal|Invalid) operands" }
 setf P		; { dg-error "(Illegal|Invalid) operands" }
 setf u		; { dg-error "(Illegal|Invalid) operands" }
 clearf U	; { dg-error "(Illegal|Invalid) operands" }
 clearf d	; { dg-error "(Illegal|Invalid) operands" }
 setf D		; { dg-error "(Illegal|Invalid) operands" }
 clearf e	; { dg-error "(Illegal|Invalid) operands" }
 setf E		; { dg-error "(Illegal|Invalid) operands" }
 clearf b
 setf B
 setf m
 clearf M
