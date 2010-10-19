;# Peripheral(plugable) module test file 
.text

; Timer1 test module configured at mod. no. 3
move T1CN, #05h
move T1MD, #233
move T1CL,A[0]
; Timer2 module test plugged at mod. no. 4
move T2CFG, #12h
move T2V, #12h
move T2C, A[0]

; MAC module test plugged at 5
move MCNT, #123
move MA, #13h
move MC0, A[13]

;test the pm support 
move 15h,#13h
move 25h, 13h
move 13h, #12h

move A[13], #1234h	; PFX 2 test
move A[15], #1234

