; Offset greater than #32767 should cause an error since the offset is 
; a signed quantity.

brlt R1,R2,$32768
