; Error for flags not applicable to current arch.
; #3: Error for non-common flags for v10+v32.
; { dg-do assemble }
; { dg-options "--march=common_v10_v32" }

y:
 clearf p	; { dg-error "(Illegal|Invalid) operands" }
 setf P		; { dg-error "(Illegal|Invalid) operands" }
 setf u		; { dg-error "(Illegal|Invalid) operands" }
 clearf U	; { dg-error "(Illegal|Invalid) operands" }
 clearf d	; { dg-error "(Illegal|Invalid) operands" }
 setf D		; { dg-error "(Illegal|Invalid) operands" }
 setf z
 setf X
 clearf c
 clearf V
 setf n
 clearf i
 clearf e	; { dg-error "(Illegal|Invalid) operands" }
 setf E		; { dg-error "(Illegal|Invalid) operands" }
 clearf b	; { dg-error "(Illegal|Invalid) operands" }
 setf B		; { dg-error "(Illegal|Invalid) operands" }
 setf m		; { dg-error "(Illegal|Invalid) operands" }
 clearf M	; { dg-error "(Illegal|Invalid) operands" }
