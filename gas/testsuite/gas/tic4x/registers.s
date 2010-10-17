	;; test all register names c3x
	.text
        ;; Test the base names
        .ifdef  TEST_ALL
start:  ldi      R0,R0
        ldi      R0,R1
        ldi      R0,R2
        ldi      R0,R3
        ldi      R0,R4
        ldi      R0,R5
        ldi      R0,R6
        ldi      R0,R7
        ldi      R0,AR0
        ldi      R0,AR1
        ldi      R0,AR2
        ldi      R0,AR3
        ldi      R0,AR4
        ldi      R0,AR5
        ldi      R0,AR6
        ldi      R0,AR7
        ldi      R0,DP
        ldi      R0,IR0
        ldi      R0,IR1
        ldi      R0,BK
        ldi      R0,SP
        ldi      R0,ST
        .endif
        .ifdef	TEST_C3X
        ldi      R0,IE
        ldi      R0,IF
        ldi      R0,IOF
        .endif
        .ifdef  TEST_C4X
        ldi      R0,DIE
        ldi      R0,IIE
        ldi      R0,IIF
        .endif
        .ifdef  TEST_ALL
        ldi      R0,RS
        ldi      R0,RE
        ldi      R0,RC
        .endif
        .ifdef  TEST_C4X
        ldi      R0,R8
        ldi      R0,R9
        ldi      R0,R10
        ldi      R0,R11
        ldpe     R0,IVTP
        ldpe     R0,TVTP
        .endif

        ;; Test the alternative names
        .ifdef TEST_ALL
        ldf      F0,F0
        ldf      F0,F1
        ldf      F0,F2
        ldf      F0,F3
        ldf      F0,F4
        ldf      F0,F5
        ldf      F0,F6
        ldf      F0,F7
        .endif
        .ifdef  TEST_C4X
        ldf      F0,F8
        ldf      F0,F9
        ldf      F0,F10
        ldf      F0,F11
        .endif
       	.end
