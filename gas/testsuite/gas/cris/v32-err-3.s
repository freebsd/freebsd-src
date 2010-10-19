; Error for flags not applicable to current arch.
; #1: Error for pre-v32 flags for v32.
; { dg-do assemble }
; { dg-options " --underscore --march=v32" }

y:
 clearf d	; { dg-error "(Illegal|Invalid) operands" }
 setf D		; { dg-error "(Illegal|Invalid) operands" }
 setf m		; { dg-error "(Illegal|Invalid) operands" }
 clearf M	; { dg-error "(Illegal|Invalid) operands" }
