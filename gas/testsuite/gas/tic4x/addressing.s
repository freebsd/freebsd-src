        ;; 
        ;; test all addressing modes and register constraints
        ;; (types/classes is read from include/opcodes/tic4x.h)
        ;;
	.text
start:
        
        ;;
        ;; Type B - infix condition branch
        ;;
Type_BI:bu      Type_BI         ; Unconditional branch (00000)
        bc      Type_BI         ; Carry branch (00001)
        blo     Type_BI         ; Lower than branch (00001)
        bls     Type_BI         ; Lower than or same branch (00010)
        bhi     Type_BI         ; Higher than branch (00011)
        bhs     Type_BI         ; Higher than or same branch (00100)
        bnc     Type_BI         ; No carry branch (00100)
        beq     Type_BI         ; Equal to branch (00101)
        bz      Type_BI         ; Zero branch (00101)
        bne     Type_BI         ; Not equal to branch (00110)
        bnz     Type_BI         ; Not zero branch (00110)
        blt     Type_BI         ; Less than branch (00111)
        bn      Type_BI         ; Negative branch (00111)
        ble     Type_BI         ; Less than or equal to branch (01000)
        bgt     Type_BI         ; Greater than branch (01001)
        bp      Type_BI         ; Positive branch (01001)
        bge     Type_BI         ; Greater than or equal branch (01010)
        bnn     Type_BI         ; Nonnegative branch (01010)
        bnv     Type_BI         ; No overflow branch (01000)
        bv      Type_BI         ; Overflow branch (01101)
        bnuf    Type_BI         ; No underflow branch (01110)
        buf     Type_BI         ; Underflow branch (01111)
        bnlv    Type_BI         ; No latched overflow branch (10000)
        blv     Type_BI         ; Latched overflow branch (10001)
        bnluf   Type_BI         ; No latched FP underflow branch (10010)
        bluf    Type_BI         ; Latched FP underflow branch (10011)
        bzuf    Type_BI         ; Zero or FP underflow branch (10100)
        b       Type_BI         ; Unconditional branch (00000)

        ;;
        ;; Type C - infix condition load
        ;;
Type_CI:ldiu    R0,R0           ; Unconditional load (00000)
        ldic    R0,R0           ; Carry load (00001)
        ldilo   R0,R0           ; Lower than load (00001)
        ldils   R0,R0           ; Lower than or same load (00010)
        ldihi   R0,R0           ; Higher than load (00011)
        ldihs   R0,R0           ; Higher than or same load (00100)
        ldinc   R0,R0           ; No carry load (00100)
        ldieq   R0,R0           ; Equal to load (00101)
        ldiz    R0,R0           ; Zero load (00101)
        ldine   R0,R0           ; Not equal to load (00110)
        ldinz   R0,R0           ; Not zero load (00110)
        ldil    R0,R0           ; Less than load (00111)
        ldin    R0,R0           ; Negative load (00111)
        ldile   R0,R0           ; Less than or equal to load (01000)
        ldigt   R0,R0           ; Greater than load (01001)
        ldip    R0,R0           ; Positive load (01001)
        ldige   R0,R0           ; Greater than or equal load (01010)
        ldinn   R0,R0           ; Nonnegative load (01010)
        ldinv   R0,R0           ; No overflow load (01000)
        ldiv    R0,R0           ; Overflow load (01101)
        ldinuf  R0,R0           ; No underflow load (01110)
        ldiuf   R0,R0           ; Underflow load (01111)
        ldinlv  R0,R0           ; No latched overflow load (10000)
        ldilv   R0,R0           ; Latched overflow load (10001)
        ldinluf R0,R0           ; No latched FP underflow load (10010)
        ldiluf  R0,R0           ; Latched FP underflow load (10011)
        ldizuf  R0,R0           ; Zero or FP underflow load (10100)

        ;;
        ;; Type * - Indirect (full)
        ;;
Type_ind:       
        ldi     *AR0,R0         ; Indirect addressing (G=10)
        ldi     *+AR0(5),R0     ;   with predisplacement add
        ldi     *-AR0(5),R0     ;   with predisplacement subtract
        ldi     *++AR0(5),R0    ;   with predisplacement add and modify
        ldi     *--AR0(5),R0    ;   with predisplacement subtract and modify
        ldi     *AR0++(5),R0    ;   with postdisplacement add and modify
        ldi     *AR0--(5),R0    ;   with postdisplacement subtract and modify
        ldi     *AR0++(5)%,R0   ;   with postdisplacement add and circular modify
        ldi     *AR0--(5)%,R0   ;   with postdisplacement subtract and circular modify
        ldi     *+AR0(IR0),R0   ;   with predisplacement add
        ldi     *-AR0(IR0),R0   ;   with predisplacement subtract
        ldi     *++AR0(IR0),R0  ;   with predisplacement add and modify
        ldi     *--AR0(IR0),R0  ;   with predisplacement subtract and modify
        ldi     *AR0++(IR0),R0  ;   with postdisplacement add and modify
        ldi     *AR0--(IR0),R0  ;   with postdisplacement subtract and modify
        ldi     *AR0++(IR0)%,R0 ;   with postdisplacement add and circular modify
        ldi     *AR0--(IR0)%,R0 ;   with postdisplacement subtract and circular modify
        ldi     *AR0++(IR0)B,R0 ;   with postincrement add and bit-reversed modify
        ldi     *AR0++,R0       ; Same as *AR0++(1)

        ;;
        ;; Type # - Direct for ldp
        ;;
Type_ldp:       
        ldp     12
        ldp     @start
        ldp     start

        ;;
        ;; Type @ - Direct
        ;;
Type_dir:       
        ldi     @start,R0
        ldi     start,R0
        ldi     @16,R0
        ldi     @65535,R0

        ;;
        ;; Type A - Address register
        ;;
Type_A: dbc     AR0,R0
        dbc     AR2,R0
        dbc     AR7,R0

        ;;
        ;; Type B - Unsigned integer (PC)
        ;;
Type_B: br      start
        br      0x809800

        ;;
        ;; Type C - Indirect
        ;;
        .ifdef TEST_C4X
Type_C: addc3   *+AR0(5),R0,R0
        .endif

        ;;
        ;; Type E - Register (all)
        ;;
Type_E: andn3   R0,R0,R0
        andn3   AR0,R0,R0
        addc3   DP,R0,R0
        andn3   R7,R0,R0

        ;;
        ;; Type e - Register (0-11)
        ;;
Type_ee:subf3   R7,R0,R0
        addf3   R0,R0,R0
        addf3   R7,R0,R0
        cmpf3   R7,R0
        .ifdef TEST_C4X
        addf3   R11,R0,R0
        .endif
        
        ;;
        ;; Type F - Short float immediate
        ;;
Type_F: ldf     0,R0
        ldf     3.5,R0
        ldf     -3.5,R0
        ldf     0e-3.5e-1,R0

        ;;
        ;; Type G - Register (all)
        ;;
Type_G: andn3   R0,AR0,R0
        addc3   R0,DP,R0
        addc3   R0,R0,R0
        andn3   R0,R7,R0

        ;;
        ;; Type g - Register (0-11)
        ;; 
Type_gg:subf3   R0,R7,R0
        addf3   R0,R0,R0
        addf3   R0,R7,R0
        cmpf3   R0,R7
        .ifdef  TEST_C4X
        addf3   R0,R11,R0
        .endif
        
        ;;
        ;; Type H - Register (0-7)
        ;;
Type_H: stf     R0,*AR0 &|| stf R0,*AR0
        stf     R0,*AR0 &|| stf R2,*AR0
        stf     R0,*AR0 &|| stf R7,*AR0

        ;;
        ;; Type I - Indirect
        ;;
Type_I: addf3   *AR0,R0,R0      ; Indirect addressing (G=10)
        addf3   *+AR0(1),R0,R0  ;   with predisplacement add
        addf3   *-AR0(1),R0,R0  ;   with predisplacement subtract
        addf3   *++AR0(1),R0,R0 ;   with predisplacement add and modify
        addf3   *--AR0(1),R0,R0 ;   with predisplacement subtract and modify
        addf3   *AR0++(1),R0,R0 ;   with postdisplacement add and modify
        addf3   *AR0--(1),R0,R0 ;   with postdisplacement subtract and modify
        addf3   *AR0++(1)%,R0,R0;   with postdisplacement add and circular modify
        addf3   *AR0--(1)%,R0,R0;   with postdisplacement subtract and circular modify
        addf3   *+AR0(IR0),R0,R0;   with predisplacement add
        addf3   *-AR0(IR0),R0,R0;   with predisplacement subtract
        addf3   *++AR0(IR0),R0,R0;  with predisplacement add and modify
        addf3   *--AR0(IR0),R0,R0;  with predisplacement subtract and modify
        addf3   *AR0++(IR0),R0,R0;  with postdisplacement add and modify
        addf3   *AR0--(IR0),R0,R0;  with postdisplacement subtract and modify
        addf3   *AR0++(IR0)%,R0,R0; with postdisplacement add and circular modify
        addf3   *AR0--(IR0)%,R0,R0; with postdisplacement subtract and circular modify
        addf3   *AR0++(IR0)B,R0,R0; with postincrement add and bit-reversed modify
        addf3   *AR0++,R0,R0    ; Same as *AR0++(1)

        ;;
        ;; Type J - Indirect
        ;;
Type_J: addf3   R0,*AR0,R0      ; Indirect addressing (G=10)
        addf3   R0,*+AR0(1),R0  ;   with predisplacement add
        addf3   R0,*-AR0(1),R0  ;   with predisplacement subtract
        addf3   R0,*++AR0(1),R0 ;   with predisplacement add and modify
        addf3   R0,*--AR0(1),R0 ;   with predisplacement subtract and modify
        addf3   R0,*AR0++(1),R0 ;   with postdisplacement add and modify
        addf3   R0,*AR0--(1),R0 ;   with postdisplacement subtract and modify
        addf3   R0,*AR0++(1)%,R0;   with postdisplacement add and circular modify
        addf3   R0,*AR0--(1)%,R0;   with postdisplacement subtract and circular modify
        addf3   R0,*+AR0(IR0),R0;   with predisplacement add
        addf3   R0,*-AR0(IR0),R0;   with predisplacement subtract
        addf3   R0,*++AR0(IR0),R0;  with predisplacement add and modify
        addf3   R0,*--AR0(IR0),R0;  with predisplacement subtract and modify
        addf3   R0,*AR0++(IR0),R0;  with postdisplacement add and modify
        addf3   R0,*AR0--(IR0),R0;  with postdisplacement subtract and modify
        addf3   R0,*AR0++(IR0)%,R0; with postdisplacement add and circular modify
        addf3   R0,*AR0--(IR0)%,R0; with postdisplacement subtract and circular modify
        addf3   R0,*AR0++(IR0)B,R0; with postincrement add and bit-reversed modify
        addf3   R0,*AR0++,R0    ; Same as *AR0++(1)

        ;;
        ;; Type K - Register (0-7)
        ;;
Type_K: ldf     *AR0,R0 &|| ldf *AR0,R1
        ldf     *AR0,R0 &|| ldf *AR0,R2
        ldf     *AR0,R0 &|| ldf *AR0,R7

        ;;
        ;; Type L - Register (0-7)
        ;;
Type_L: stf     R0,*AR0 &|| stf R0,*AR0
        stf     R2,*AR0 &|| stf R0,*AR0
        stf     R7,*AR0 &|| stf R0,*AR0

        ;;
        ;; Type M - Register (2-3)
        ;; 
Type_M: mpyf3   *AR0,*AR0,R0 &|| addf3 R0,R0,R2
        mpyf3   *AR0,*AR0,R0 &|| addf3 R0,R0,R3
        
        ;;
        ;; Type N - Register (0-1)
        ;;
Type_N: mpyf3   *AR0,*AR0,R0 &|| addf3 R0,R0,R2
        mpyf3   *AR0,*AR0,R1 &|| addf3 R0,R0,R2

        ;;
        ;; Type O - Indirect
        ;;
        .ifdef TEST_C4X
Type_O: addc3   *+AR0(5),*+AR0(5),R0
        .endif
        
        ;;
        ;; Type P - Displacement (PC rel)
        ;; 
Type_P: callc   start
        callc   1

        ;;
        ;; Type Q - Register (all)
        ;;
Type_Q: ldi     R0,R0
        ldi     AR0,R0
        ldi     DP,R0
        ldi     SP,R0

        ;;
        ;; Type q - Register (0-11)
        ;;
Type_qq:fix     R0,R0
        fix     R7,R0
        .ifdef  TEST_C4X
        fix     R11,R0
        absf    R11,R0
        .endif

        ;;
        ;; Type R - Register (all)
        ;;
Type_R: ldi     R0,R0
        ldi     R0,AR0
        ldi     R0,DP
        ldi     R0,SP

        ;;
        ;; Type r - Register (0-11)
        ;;
Type_rr:ldf     R0,R0
        ldf     R0,R7
        .ifdef  TEST_C4X
        ldf     R0,R11
        .endif

        ;;
        ;; Type S - Signed immediate
        ;;
Type_S: ldi     0,R0
        ldi     -123,R0
        ldi     6543,R0
        ldi     -32768, R0

        ;;
        ;; Type T - Integer
        ;;
        .ifdef  TEST_C4X
Type_T: stik    0,*AR0
        stik    12,*AR0
        stik    -5,*AR0
        .endif

        ;;
        ;; Type U - Unsigned integer
        ;;
Type_U: and     0,R0
        and     256,R0
        and     65535,R0

        ;;
        ;; Type V - Vector
        ;;
Type_V: trapu   12
        trapu   0
        trapu   31
        .ifdef  TEST_C4X
        trapu   511
        .endif

        ;;
        ;; Type W - Short int
        ;;
        .ifdef  TEST_C4X
Type_W: addc3   -3,R0,R0
        addc3   5,R0,R0
        .endif

        ;;
        ;; Type X - Expansion register
        ;;
        .ifdef  TEST_C4X
Type_X: ldep    IVTP,R0
        ldep    TVTP,R0
        .endif
        
        ;;
        ;; Type Y - Address register
        ;;
        .ifdef  TEST_C4X
Type_Y: lda     R0,AR0
        lda     R0,DP
        lda     R0,SP
        lda     R0,IR0
        .endif

        ;;
        ;; Type Z - Expansion register
        ;;
        .ifdef  TEST_C4X
Type_Z: ldpe    R0,IVTP
        ldpe    R0,TVTP
        .endif
