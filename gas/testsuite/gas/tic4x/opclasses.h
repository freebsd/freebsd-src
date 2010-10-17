/* Opcode infix
   B  condition              16--20   U,C,Z,LO,HI, etc.
   C  condition              23--27   U,C,Z,LO,HI, etc.

   Arguments
   ,  required arg follows
   ;  optional arg follows

   Argument types             bits    [classes] - example
   -----------------------------------------------------------
   *  indirect (all)          0--15   [A,AB,AU,AF,A2,A3,A6,A7,AY,B,BA,BB,BI,B6,B7] - *+AR0(5), *++AR0(IR0)
   #  direct (for LDP)        0--15   [Z] - @start, start
   @  direct                  0--15   [A,AB,AU,AF,A3,A6,A7,AY,B,BA,BB,BI,B6,B7] - @start, start
   A  address register       22--24   [D] - AR0, AR7
   B  unsigned integer        0--23   [I,I2] - @start, start  (absolute on C3x, relative on C4x)
   C  indirect (disp - C4x)   0--7    [S,SC,S2,T,TC,T2,T2C] - *+AR0(5)
   E  register (all)          0--7    [T,TC,T2,T2C] - R0, R7, R11, AR0, DP
   e  register (0-11)         0--7    [S,SC,S2] - R0, R7, R11
   F  short float immediate   0--15   [AF,B,BA,BB] - 3.5, 0e-3.5e-1
   G  register (all)          8--15   [T,TC,T2,T2C] - R0, R7, R11, AR0, DP
   g  register (0-11)         0--7    [S,SC,S2] - R0, R7, R11
   H  register (0-7)         18--16   [LS,M,P,Q] - R0, R7
   I  indirect (no disp)      0--7    [S,SC,S2,T,TC,T2,T2C] - *+AR0(1), *+AR0(IR0)
   i  indirect (enhanced)     0--7    [LL,LS,M,P,Q,QC] - *+AR0(1), R5
   J  indirect (no disp)      8--15   [LL,LS,P,Q,QC,S,SC,S2,T,TC,T2,T2C] - *+AR0(1), *+AR0(IR0)
   j  indirect (enhanced)     8--15   [M] - *+AR0(1), R5
   K  register               19--21   [LL,M,Q,QC] - R0, R7
   L  register               22--24   [LL,LS,P,Q,QC] - R0, R7
   M  register (R2,R3)       22--22   [M] R2, R3
   N  register (R0,R1)       23--23   [M] R0, R1
   O  indirect(disp - C4x)    8--15   [S,SC,S2,T,TC,T2] - *+AR0(5)
   P  displacement (PC Rel)   0--15   [D,J,JS] - @start, start
   Q  register (all)          0--15   [A,AB,AU,A2,A3,AY,BA,BI,D,I2,J,JS] - R0, AR0, DP, SP
   q  register (0-11)         0--15   [AF,B,BB] - R0, R7, R11
   R  register (all)         16--20   [A,AB,AU,AF,A6,A7,R,T,TC] - R0, AR0, DP, SP
   r  register (0-11)        16--20   [B,BA,BB,BI,B6,B7,RF,S,SC] - R0, R1, R11
   S  short int immediate     0--15   [A,AB,AY,BI] - -5, 5
   T  integer (C4x)          16--20   [Z] - -5, 12
   U  unsigned integer        0--15   [AU,A3] - 0, 65535
   V  vector (C4x: 0--8)      0--4    [Z] - 25, 7
   W  short int (C4x)         0--7    [T,TC,T2,T2C] - -3, 5
   X  expansion reg (C4x)     0--4    [Z] - IVTP, TVTP
   Y  address reg (C4x)      16--20   [Z] - AR0, DP, SP, IR0
   Z  expansion reg (C4x)    16--20   [Z] - IVTP, TVTP
*/

/* A: General 2-operand integer operations
   Syntax: <i> src, dst
      src = Register (Q), Direct (@), Indirect (*), Signed immediate (S)
      dst = Register (R)
   Instr: 15/8 - ABSI, ADDC, ADDI, ASH, CMPI, LDI, LSH, MPYI, NEGB, NEGI,
                SUBB, SUBC, SUBI, SUBRB, SUBRI, C4x: LBn, LHn, LWLn, LWRn,
                MBn, MHn, MPYSHI, MPYUHI
*/
#define A_CLASS(name, level) \
  .ifdef level                    &\
name##_A:                         &\
  name  AR1, AR0        /* Q;R */ &\
  name  AR0             /* Q;R */ &\
  name  @start, AR0     /* @,R */ &\
  name  *+AR0(5), AR0   /* *,R */ &\
  name  -5, AR0         /* S,R */ &\
  .endif


/* AB: General 2-operand integer operation with condition
   Syntax: <i>c src, dst
       c   = Condition
       src = Register (Q), Direct (@), Indirect (*), Signed immediate (S)
       dst = Register (R)
   Instr: 1/0 - LDIc
*/
#define AB_CLASS(name, level) \
  .ifdef level                    &\
name##_AB:                        &\
  name  AR1, AR0        /* Q;R */ &\
  name  AR0             /* Q;R */ &\
  name  @start, AR0     /* @,R */ &\
  name  *+AR0(5), AR0   /* *,R */ &\
  name  -5, AR0         /* S,R */ &\
  .endif


/* AU: General 2-operand unsigned integer operation
   Syntax: <i> src, dst
        src = Register (Q), Direct (@), Indirect (*), Unsigned immediate (U)
        dst = Register (R)
   Instr: 6/2 - AND, ANDN, NOT, OR, TSTB, XOR, C4x: LBUn, LHUn
*/
#define AU_CLASS(name, level) \
  .ifdef level                    &\
name##_AU:                        &\
  name  AR1, AR0        /* Q;R */ &\
  name  AR0             /* Q;R */ &\
  name  @start, AR0     /* @,R */ &\
  name  *+AR0(5), AR0   /* *,R */ &\
  name  5, AR0          /* U,R */ &\
  .endif


/* AF: General 2-operand float to integer operation
   Syntax: <i> src, dst
        src = Register 0-11 (q), Direct (@), Indirect (*), Float immediate (F)
        dst = Register (R)
   Instr: 1/0 - FIX
*/
#define AF_CLASS(name, level) \
  .ifdef level                    &\
name##_AF:                        &\
  name  R1, R0          /* q;R */ &\
  name  R0              /* q;R */ &\
  name  @start, AR0     /* @,R */ &\
  name  *+AR0(5), AR0   /* *,R */ &\
  name  3.5, AR0        /* F,R */ &\
  .endif


/* A2: Limited 1-operand (integer) operation
   Syntax: <i> src
       src = Register (Q), Indirect (*), None
   Instr: 1/0 - NOP
*/
#define A2_CLASS(name, level) \
  .ifdef level                    &\
name##_A2:                        &\
  name  AR0               /* Q */ &\
  name  *+AR0(5)          /* * */ &\
  name                    /*   */ &\
  .endif


/* A3: General 1-operand unsigned integer operation
   Syntax: <i> src
        src = Register (Q), Direct (@), Indirect (*), Unsigned immediate (U)
   Instr: 1/0 - RPTS
*/
#define A3_CLASS(name, level) \
  .ifdef level                    &\
name##_A3:                        &\
  name  AR1               /* Q */ &\
  name  @start            /* @ */ &\
  name  *+AR0(5)          /* * */ &\
  name  5                 /* U */ &\
  .endif


/* A6: Limited 2-operand integer operation
   Syntax: <i> src, dst
       src = Direct (@), Indirect (*)
       dst = Register (R)
   Instr: 1/1 - LDII, C4x: SIGI
*/
#define A6_CLASS(name, level) \
  .ifdef level                    &\
name##_A6:                        &\
  name  @start, AR0     /* @,R */ &\
  name  *+AR0(5), AR0   /* *,R */ &\
  .endif


/* A7: Limited 2-operand integer store operation
   Syntax: <i> src, dst
       src = Register (R)
       dst = Direct (@), Indirect (*)
   Instr: 2/0 - STI, STII
*/
#define A7_CLASS(name, level) \
  .ifdef level                    &\
name##_A7:                        &\
  name  AR0, @start     /* R,@ */ &\
  name  AR0, *+AR0(5)   /* R,* */ &\
  .endif


/* AY: General 2-operand signed address load operation
   Syntax: <i> src, dst
        src = Register (Q), Direct (@), Indirect (*), Signed immediate (S)
        dst = Address register - ARx, IRx, DP, BK, SP (Y)
   Instr: 0/1 - C4x: LDA
   Note: Q and Y should *never* be the same register
*/
#define AY_CLASS(name, level) \
  .ifdef level                    &\
name##_AY:                        &\
  name  AR1, AR0        /* Q,Y */ &\
  name  @start, AR0     /* @,Y */ &\
  name  *+AR0(5), AR0   /* *,Y */ &\
  name  -5, AR0         /* S,Y */ &\
  .endif


/* B: General 2-operand float operation
   Syntax: <i> src, dst
       src = Register 0-11 (q), Direct (@), Indirect (*), Float immediate (F)
       dst = Register 0-11 (r)
   Instr: 12/2 - ABSF, ADDF, CMPF, LDE, LDF, LDM, MPYF, NEGF, NORM, RND,
                 SUBF, SUBRF, C4x: RSQRF, TOIEEE
*/
#define B_CLASS(name, level) \
  .ifdef level                    &\
name##_B:                         &\
  name  R1, R0          /* q;r */ &\
  name  R0              /* q;r */ &\
  name  @start, R0      /* @,r */ &\
  name  *+AR0(5), R0    /* *,r */ &\
  name  3.5, R0         /* F,r */ &\
  .endif


/* BA: General 2-operand integer to float operation
   Syntax: <i> src, dst
       src = Register (Q), Direct (@), Indirect (*), Float immediate (F)
       dst = Register 0-11 (r)
   Instr: 0/1 - C4x: CRCPF
*/
#define BA_CLASS(name, level) \
  .ifdef level                    &\
name##_BA:                        &\
  name  AR1, R0         /* Q;r */ &\
  name  R0              /* Q;r */ &\
  name  @start, R0      /* @,r */ &\
  name  *+AR0(5), R0    /* *,r */ &\
  name  3.5, R0         /* F,r */ &\
  .endif


/* BB: General 2-operand conditional float operation
   Syntax: <i>c src, dst
       c   = Condition
       src = Register 0-11 (q), Direct (@), Indirect (*), Float immediate (F)
       dst = Register 0-11 (r)
   Instr: 1/0 - LDFc
*/
#define BB_CLASS(name, level) \
  .ifdef level                    &\
name##_BB:                        &\
  name  R1, R0          /* q;r */ &\
  name  R0              /* q;r */ &\
  name  @start, R0      /* @,r */ &\
  name  *+AR0(5), R0    /* *,r */ &\
  name  3.5, R0         /* F,r */ &\
  .endif


/* BI: General 2-operand integer to float operation (yet different to BA)
   Syntax: <i> src, dst
       src = Register (Q), Direct (@), Indirect (*), Signed immediate (S)
       dst = Register 0-11 (r)
   Instr: 1/0 - FLOAT
*/
#define BI_CLASS(name, level) \
  .ifdef level                    &\
name##_BI:                        &\
  name  AR1, R0         /* Q;r */ &\
  name  R0              /* Q;r */ &\
  name  @start, R0      /* @,r */ &\
  name  *+AR0(5), R0    /* *,r */ &\
  name  -5, R0          /* S,r */ &\
  .endif


/* B6: Limited 2-operand float operation 
   Syntax: <i> src, dst
       src = Direct (@), Indirect (*)
       dst = Register 0-11 (r)
   Instr: 1/1 - LDFI, C4x: FRIEEE
*/
#define B6_CLASS(name, level) \
  .ifdef level                    &\
name##_B6:                        &\
  name  @start, R0      /* @,r */ &\
  name  *+AR0(5), R0    /* *,r */ &\
  .endif


/* B7: Limited 2-operand float store operation
   Syntax: <i> src, dst
       src = Register 0-11 (r)
       dst = Direct (@), Indirect (*)
   Instr: 2/0 - STF, STFI
*/
#define B7_CLASS(name, level) \
  .ifdef level                    &\
name##_B7:                        &\
  name  R0, @start      /* r,@ */ &\
  name  R0, *+AR0(5)    /* r,* */ &\
  .endif


/* D: Decrement and brach operations
   Syntax: <i>c ARn, dst
       c   = condition
       ARn = AR register 0-7 (A)
       dst = Register (Q), PC-relative (P)
   Instr: 2/0 - DBc, DBcD
   Alias: <namea> <nameb>
*/
#define D_CLASS(namea, nameb, level) \
  .ifdef level                    &\
namea##_D:                        &\
  namea  AR0, R0        /* A,Q */ &\
  namea  AR0, start     /* A,P */ &\
nameb##_D:                        &\
  nameb  AR0, R0        /* A,Q */ &\
  nameb  AR0, start     /* A,P */ &\
  .endif


/* J: General conditional branch operations
   Syntax: <i>c dst
       c   = Condition
       dst = Register (Q), PC-relative (P)
   Instr: 2/3 - Bc, BcD, C4x: BcAF, BcAT, LAJc
   Alias: <namea> <nameb>
*/
#define J_CLASS(namea, nameb, level) \
  .ifdef level                    &\
namea##_J:                        &\
  namea  R0               /* Q */ &\
  namea  start            /* P */ &\
nameb##_J:                        &\
  nameb  R0               /* Q */ &\
  nameb  start            /* P */ &\
  .endif


/* LL: Load-load parallell operation
   Syntax: <i> src2, dst2 || <i> src1, dst1
       src1 = Indirect 0,1,IR0,IR1 (J)
       dst1 = Register 0-7 (K)
       src2 = Indirect 0,1,IR0,IR1, ENH: Register (i)
       dst2 = Register 0-7 (L)
   Instr: 2/0 - LDF||LDF, LDI||LDI
   Alias: i||i, i1||i2, i2||i1
*/
#define LL_CLASS(name, level) \
  .ifdef level                                                      &\
name##_LL:                                                          &\
  name     *+AR0(1), R0  &||  name     *+AR1(1), R1   /* i;L|J,K */ &\
  name##2  *+AR0(1), R0  &||  name##1  *+AR1(1), R1   /* i;L|J,K */ &\
  name##1  *+AR1(1), R1  &||  name##2  *+AR0(1), R0   /* J,K|i;L */ &\
  .endif                                                            &\
  .ifdef TEST_ENH                                                   &\
name##_LL_enh:                                                      &\
  name     R0, R0        &||  name     *+AR1(1), R1   /* i;L|J,K */ &\
  name     R0            &||  name     *+AR1(1), R1   /* i;L|J,K */ &\
  name##2  R0, R0        &||  name##1  *+AR1(1), R1   /* i;L|J,K */ &\
  name##2  R0            &||  name##1  *+AR1(1), R1   /* i;L|J,K */ &\
  name##1  *+AR1(1), R1  &||  name##2  R0, R0         /* J,K|i;L */ &\
  name##1  *+AR1(1), R1  &||  name##2  R0             /* J,K|i;L */ &\
  .endif



/* LS: Store-store parallell operation
   Syntax: <i> src2, dst2 || <i> src1, dst1
       src1 = Register 0-7 (H)
       dst1 = Indirect 0,1,IR0,IR1 (J)
       src2 = Register 0-7 (L)
       dst2 = Indirect 0,1,IR0,IR1, ENH: register (i)
   Instr: 2/0 - STF||STF, STI||STI
   Alias: i||i, i1||i2, i2||i1.
*/
#define LS_CLASS(name, level) \
  .ifdef level                                                      &\
name##_LS:                                                          &\
  name     R0, *+AR0(1)  &||  name     R1, *+AR1(1)   /* L;i|H,J */ &\
  name##2  R0, *+AR0(1)  &||  name##1  R1, *+AR1(1)   /* L;i|H,J */ &\
  name##1  R1, *+AR1(1)  &||  name##2  R0, *+AR0(1)   /* H,J|L;i */ &\
  .endif                                                            &\
  .ifdef TEST_ENH                                                   &\
name##_LS_enh:                                                      &\
  name     R0, R0        &||  name     R1, *+AR1(1)   /* L;i|H,J */ &\
  name     R0            &||  name     R1, *+AR1(1)   /* L;i|H,J */ &\
  name##2  R0, R0        &||  name##1  R1, *+AR1(1)   /* L;i|H,J */ &\
  name##2  R0            &||  name##1  R1, *+AR1(1)   /* L;i|H,J */ &\
  name##1  R1, *+AR1(1)  &||  name##2  R0, R0         /* H,J|L;i */ &\
  name##1  R1, *+AR1(1)  &||  name##2  R0             /* H,J|L;i */ &\
  .endif


/* M: General multiply and add/sub operations
   Syntax: <ia> src3,src4,dst1 || <ib> src2,src1,dst2 [00] - Manual
           <ia> src3,src1,dst1 || <ib> src2,src4,dst2 [01] - Manual
           <ia> src1,src3,dst1 || <ib> src2,src4,dst2 [01]
           <ia> src1,src2,dst1 || <ib> src4,src3,dst2 [02] - Manual
           <ia> src3,src1,dst1 || <ib> src4,src2,dst2 [03] - Manual
           <ia> src1,src3,dst1 || <ib> src4,src2,dst2 [03]
       src1 = Register 0-7 (K)
       src2 = Register 0-7 (H)
       src3 = Indirect 0,1,IR0,IR1, ENH: register (j)
       src4 = Indirect 0,1,IR0,IR1, ENH: register (i)
       dst1 = Register 0-1 (N)
       dst2 = Register 2-3 (M)
   Instr: 4/0 - MPYF3||ADDF3, MPYF3||SUBF3, MPYI3||ADDI3, MPYI3||SUBI3
   Alias: a||b, a3||n, a||b3, a3||b3, b||a, b3||a, b||a3, b3||a3
*/
#define M_CLASS(namea, nameb, level) \
  .ifdef level                                                                                &\
namea##_##nameb##_M:                                                                          &\
  namea     *+AR0(1), *+AR1(1), R0  &||  nameb     R0, R1, R2               /* i;j;N|H;K;M */ &\
  namea     *+AR0(1), *+AR1(1), R0  &||  nameb     R0, R2                   /* i;j;N|H;K;M */ &\
  namea     *+AR0(1), R0, R0        &||  nameb     R0, *+AR1(1), R2         /* j;K;N|H;i;M */ &\
  namea     *+AR0(1), R0            &||  nameb     R0, *+AR1(1), R2         /* j;K;N|H;i;M */ &\
  namea     R0, *+AR0(1), R0        &||  nameb     R0, *+AR1(1), R2         /* K;j;N|H;i;M */ &\
  namea     R2, R1, R0              &||  nameb     *+AR0(1), *+AR1(1), R2   /* H;K;N|i;j;M */ &\
  namea     R2, R0                  &||  nameb     *+AR0(1), *+AR1(1), R2   /* H;K;N|i;j;M */ &\
  namea     *+AR0(1), R1, R0        &||  nameb     *+AR1(1), R3, R2         /* j;K;N|i;H;M */ &\
  namea     *+AR0(1), R0            &||  nameb     *+AR1(1), R3, R2         /* j;K;N|i;H;M */ &\
  namea     *+AR0(1), R1, R0        &||  nameb     *+AR1(1), R2             /* j;K;N|i;H;M */ &\
  namea     *+AR0(1), R0            &||  nameb     *+AR1(1), R2             /* j;K;N|i;H;M */ &\
  namea     R0, *+AR0(1), R0        &||  nameb     *+AR1(1), R0, R2         /* K;j;N|i;H;M */ &\
  namea     R0, *+AR0(1), R0        &||  nameb     *+AR1(1), R2             /* K;j;N|i;H;M */ &\
  .endif                                                                                      &\
  .ifdef TEST_ENH                                                                             &\
namea##_##nameb##_M_enh:                                                                      &\
  namea     R0, R0, R0              &||  nameb     R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea     R0, R0                  &||  nameb     R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea     R0                      &||  nameb     R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea     R0, R0                  &||  nameb     R2, R2                   /* i;j;N|H;K;M */ &\
  namea     R0                      &||  nameb     R2, R2                   /* i;j;N|H;K;M */ &\
  namea     R0                      &||  nameb     R2                       /* i;j;N|H;K;M */ &\
  namea     AR0, AR0, R0            &||  nameb     R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea     AR0, R0, R0             &||  nameb     R0, AR0, R2              /* j;K;N|H;i;M */ &\
  namea     R0, AR0, R0             &||  nameb     R0, AR0, R2              /* K;j;N|H;i;M */ &\
  namea     R2, R1, R0              &||  nameb     AR0, AR1, R2             /* H;K;N|i;j;M */ &\
  namea     AR0, R1, R0             &||  nameb     AR0, R3, R2              /* j;K;N|i;H;M */ &\
  namea     R0, AR0, R0             &||  nameb     AR0, R0, R2              /* K;j;N|i;H;M */ &\
  .endif                                                                                      &\
  .ifdef level                                                                                &\
namea##3_##nameb##_M:                                                                         &\
  namea##3  *+AR0(1), *+AR1(1), R0  &||  nameb     R0, R1, R2               /* i;j;N|H;K;M */ &\
  namea##3  *+AR0(1), *+AR1(1), R0  &||  nameb     R0, R2                   /* i;j;N|H;K;M */ &\
  namea##3  *+AR0(1), R0, R0        &||  nameb     R0, *+AR1(1), R2         /* j;K;N|H;i;M */ &\
  namea##3  *+AR0(1), R0            &||  nameb     R0, *+AR1(1), R2         /* j;K;N|H;i;M */ &\
  namea##3  R0, *+AR0(1), R0        &||  nameb     R0, *+AR1(1), R2         /* K;j;N|H;i;M */ &\
  namea##3  R2, R1, R0              &||  nameb     *+AR0(1), *+AR1(1), R2   /* H;K;N|i;j;M */ &\
  namea##3  R2, R0                  &||  nameb     *+AR0(1), *+AR1(1), R2   /* H;K;N|i;j;M */ &\
  namea##3  *+AR0(1), R1, R0        &||  nameb     *+AR1(1), R3, R2         /* j;K;N|i;H;M */ &\
  namea##3  *+AR0(1), R0            &||  nameb     *+AR1(1), R3, R2         /* j;K;N|i;H;M */ &\
  namea##3  *+AR0(1), R1, R0        &||  nameb     *+AR1(1), R2             /* j;K;N|i;H;M */ &\
  namea##3  *+AR0(1), R0            &||  nameb     *+AR1(1), R2             /* j;K;N|i;H;M */ &\
  namea##3  R0, *+AR0(1), R0        &||  nameb     *+AR1(1), R0, R2         /* K;j;N|i;H;M */ &\
  namea##3  R0, *+AR0(1), R0        &||  nameb     *+AR1(1), R2             /* K;j;N|i;H;M */ &\
  .endif                                                                                      &\
  .ifdef TEST_ENH                                                                             &\
namea##3_##nameb##_M_enh:                                                                     &\
  namea##3  R0, R0, R0              &||  nameb     R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea##3  R0, R0                  &||  nameb     R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea##3  R0                      &||  nameb     R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea##3  R0, R0                  &||  nameb     R2, R2                   /* i;j;N|H;K;M */ &\
  namea##3  R0                      &||  nameb     R2, R2                   /* i;j;N|H;K;M */ &\
  namea##3  R0                      &||  nameb     R2                       /* i;j;N|H;K;M */ &\
  namea##3  AR0, AR0, R0            &||  nameb     R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea##3  AR0, R0, R0             &||  nameb     R0, AR0, R2              /* j;K;N|H;i;M */ &\
  namea##3  R0, AR0, R0             &||  nameb     R0, AR0, R2              /* K;j;N|H;i;M */ &\
  namea##3  R2, R1, R0              &||  nameb     AR0, AR1, R2             /* H;K;N|i;j;M */ &\
  namea##3  AR0, R1, R0             &||  nameb     AR0, R3, R2              /* j;K;N|i;H;M */ &\
  namea##3  R0, AR0, R0             &||  nameb     AR0, R0, R2              /* K;j;N|i;H;M */ &\
  .endif                                                                                      &\
  .ifdef level                                                                                &\
namea##_##nameb##3_M:                                                                         &\
  namea     *+AR0(1), *+AR1(1), R0  &||  nameb##3  R0, R1, R2               /* i;j;N|H;K;M */ &\
  namea     *+AR0(1), *+AR1(1), R0  &||  nameb##3  R0, R2                   /* i;j;N|H;K;M */ &\
  namea     *+AR0(1), R0, R0        &||  nameb##3  R0, *+AR1(1), R2         /* j;K;N|H;i;M */ &\
  namea     *+AR0(1), R0            &||  nameb##3  R0, *+AR1(1), R2         /* j;K;N|H;i;M */ &\
  namea     R0, *+AR0(1), R0        &||  nameb##3  R0, *+AR1(1), R2         /* K;j;N|H;i;M */ &\
  namea     R2, R1, R0              &||  nameb##3  *+AR0(1), *+AR1(1), R2   /* H;K;N|i;j;M */ &\
  namea     R2, R0                  &||  nameb##3  *+AR0(1), *+AR1(1), R2   /* H;K;N|i;j;M */ &\
  namea     *+AR0(1), R1, R0        &||  nameb##3  *+AR1(1), R3, R2         /* j;K;N|i;H;M */ &\
  namea     *+AR0(1), R0            &||  nameb##3  *+AR1(1), R3, R2         /* j;K;N|i;H;M */ &\
  namea     *+AR0(1), R1, R0        &||  nameb##3  *+AR1(1), R2             /* j;K;N|i;H;M */ &\
  namea     *+AR0(1), R0            &||  nameb##3  *+AR1(1), R2             /* j;K;N|i;H;M */ &\
  namea     R0, *+AR0(1), R0        &||  nameb##3  *+AR1(1), R0, R2         /* K;j;N|i;H;M */ &\
  namea     R0, *+AR0(1), R0        &||  nameb##3  *+AR1(1), R2             /* K;j;N|i;H;M */ &\
  .endif                                                                                      &\
  .ifdef TEST_ENH                                                                             &\
namea##_##nameb##3_M_enh:                                                                     &\
  namea     R0, R0, R0              &||  nameb##3  R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea     R0, R0                  &||  nameb##3  R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea     R0                      &||  nameb##3  R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea     R0, R0                  &||  nameb##3  R2, R2                   /* i;j;N|H;K;M */ &\
  namea     R0                      &||  nameb##3  R2, R2                   /* i;j;N|H;K;M */ &\
  namea     R0                      &||  nameb##3  R2                       /* i;j;N|H;K;M */ &\
  namea     AR0, AR0, R0            &||  nameb##3  R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea     AR0, R0, R0             &||  nameb##3  R0, AR0, R2              /* j;K;N|H;i;M */ &\
  namea     R0, AR0, R0             &||  nameb##3  R0, AR0, R2              /* K;j;N|H;i;M */ &\
  namea     R2, R1, R0              &||  nameb##3  AR0, AR1, R2             /* H;K;N|i;j;M */ &\
  namea     AR0, R1, R0             &||  nameb##3  AR0, R3, R2              /* j;K;N|i;H;M */ &\
  namea     R0, AR0, R0             &||  nameb##3  AR0, R0, R2              /* K;j;N|i;H;M */ &\
  .endif                                                                                      &\
  .ifdef level                                                                                &\
namea##3_##nameb##3_M:                                                                        &\
  namea##3  *+AR0(1), *+AR1(1), R0  &||  nameb##3  R0, R1, R2               /* i;j;N|H;K;M */ &\
  namea##3  *+AR0(1), *+AR1(1), R0  &||  nameb##3  R0, R2                   /* i;j;N|H;K;M */ &\
  namea##3  *+AR0(1), R0, R0        &||  nameb##3  R0, *+AR1(1), R2         /* j;K;N|H;i;M */ &\
  namea##3  *+AR0(1), R0            &||  nameb##3  R0, *+AR1(1), R2         /* j;K;N|H;i;M */ &\
  namea##3  R0, *+AR0(1), R0        &||  nameb##3  R0, *+AR1(1), R2         /* K;j;N|H;i;M */ &\
  namea##3  R2, R1, R0              &||  nameb##3  *+AR0(1), *+AR1(1), R2   /* H;K;N|i;j;M */ &\
  namea##3  R2, R0                  &||  nameb##3  *+AR0(1), *+AR1(1), R2   /* H;K;N|i;j;M */ &\
  namea##3  *+AR0(1), R1, R0        &||  nameb##3  *+AR1(1), R3, R2         /* j;K;N|i;H;M */ &\
  namea##3  *+AR0(1), R0            &||  nameb##3  *+AR1(1), R3, R2         /* j;K;N|i;H;M */ &\
  namea##3  *+AR0(1), R1, R0        &||  nameb##3  *+AR1(1), R2             /* j;K;N|i;H;M */ &\
  namea##3  *+AR0(1), R0            &||  nameb##3  *+AR1(1), R2             /* j;K;N|i;H;M */ &\
  namea##3  R0, *+AR0(1), R0        &||  nameb##3  *+AR1(1), R0, R2         /* K;j;N|i;H;M */ &\
  namea##3  R0, *+AR0(1), R0        &||  nameb##3  *+AR1(1), R2             /* K;j;N|i;H;M */ &\
  .endif                                                                                      &\
  .ifdef TEST_ENH                                                                             &\
namea##3_##nameb##3_M_enh:                                                                    &\
  namea##3  R0, R0, R0              &||  nameb##3  R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea##3  R0, R0                  &||  nameb##3  R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea##3  R0                      &||  nameb##3  R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea##3  R0, R0                  &||  nameb##3  R2, R2                   /* i;j;N|H;K;M */ &\
  namea##3  R0                      &||  nameb##3  R2, R2                   /* i;j;N|H;K;M */ &\
  namea##3  R0                      &||  nameb##3  R2                       /* i;j;N|H;K;M */ &\
  namea##3  AR0, AR0, R0            &||  nameb##3  R2, R2, R2               /* i;j;N|H;K;M */ &\
  namea##3  AR0, R0, R0             &||  nameb##3  R0, AR0, R2              /* j;K;N|H;i;M */ &\
  namea##3  R0, AR0, R0             &||  nameb##3  R0, AR0, R2              /* K;j;N|H;i;M */ &\
  namea##3  R2, R1, R0              &||  nameb##3  AR0, AR1, R2             /* H;K;N|i;j;M */ &\
  namea##3  AR0, R1, R0             &||  nameb##3  AR0, R3, R2              /* j;K;N|i;H;M */ &\
  namea##3  R0, AR0, R0             &||  nameb##3  AR0, R0, R2              /* K;j;N|i;H;M */ &\
  .endif                                                                                      &\
  .ifdef level                                                                                &\
nameb##_##namea##_M:                                                                          &\
  nameb     R0, R1, R2              &||  namea     *+AR0(1), *+AR1(1), R0   /* H;K;M|i;j;N */ &\
  nameb     R0, R2                  &||  namea     *+AR0(1), *+AR1(1), R0   /* H;K;M|i;j;N */ &\
  nameb     R0, *+AR1(1), R2        &||  namea     *+AR0(1), R0, R0         /* H;i;M|j;K;N */ &\
  nameb     R0, *+AR1(1), R2        &||  namea     *+AR0(1), R0             /* H;i;M|j;K;N */ &\
  nameb     R0, *+AR1(1), R2        &||  namea     R0, *+AR0(1), R0         /* H;i;M|K;j;N */ &\
  nameb     *+AR0(1), *+AR1(1), R2  &||  namea     R2, R1, R0               /* i;j;M|H;K;N */ &\
  nameb     *+AR0(1), *+AR1(1), R2  &||  namea     R2, R0                   /* i;j;M|H;K;N */ &\
  nameb     *+AR1(1), R3, R2        &||  namea     *+AR0(1), R1, R0         /* i;H;M|j;K;N */ &\
  nameb     *+AR1(1), R3, R2        &||  namea     *+AR0(1), R0             /* i;H;M|j;K;N */ &\
  nameb     *+AR1(1), R2            &||  namea     *+AR0(1), R1, R0         /* i;H;M|j;K;N */ &\
  nameb     *+AR1(1), R2            &||  namea     *+AR0(1), R0             /* i;H;M|j;K;N */ &\
  nameb     *+AR1(1), R0, R2        &||  namea     R0, *+AR0(1), R0         /* i;H;M|K;j;N */ &\
  nameb     *+AR1(1), R2            &||  namea     R0, *+AR0(1), R0         /* i;H;M|K;j;N */ &\
  .endif                                                                                      &\
  .ifdef TEST_ENH                                                                             &\
nameb##_##namea##_M_enh:                                                                      &\
  nameb     R2, R2, R2              &||  namea     R0, R0, R0               /* H;K;M|i;j;N */ &\
  nameb     R2, R2, R2              &||  namea     R0, R0                   /* H;K;M|i;j;N */ &\
  nameb     R2, R2, R2              &||  namea     R0                       /* H;K;M|i;j;N */ &\
  nameb     R2, R2                  &||  namea     R0, R0                   /* H;K;M|i;j;N */ &\
  nameb     R2, R2                  &||  namea     R0                       /* H;K;M|i;j;N */ &\
  nameb     R2                      &||  namea     R0                       /* H;K;M|i;j;N */ &\
  nameb     R2, R2, R2              &||  namea     AR0, AR0, R0             /* H;K;M|i;j;N */ &\
  nameb     R0, AR0, R2             &||  namea     AR0, R0, R0              /* H;i;M|j;K;N */ &\
  nameb     R0, AR0, R2             &||  namea     R0, AR0, R0              /* H;i;M|K;j;N */ &\
  nameb     AR0, AR1, R2            &||  namea     R2, R1, R0               /* i;j;M|H;K;N */ &\
  nameb     AR0, R3, R2             &||  namea     AR0, R1, R0              /* i;H;M|j;K;N */ &\
  nameb     AR0, R0, R2             &||  namea     R0, AR0, R0              /* i;H;M|K;j;N */ &\
  .endif                                                                                      &\
  .ifdef level                                                                                &\
nameb##3_##namea##_M:                                                                         &\
  nameb##3  R0, R1, R2              &||  namea     *+AR0(1), *+AR1(1), R0   /* H;K;M|i;j;N */ &\
  nameb##3  R0, R2                  &||  namea     *+AR0(1), *+AR1(1), R0   /* H;K;M|i;j;N */ &\
  nameb##3  R0, *+AR1(1), R2        &||  namea     *+AR0(1), R0, R0         /* H;i;M|j;K;N */ &\
  nameb##3  R0, *+AR1(1), R2        &||  namea     *+AR0(1), R0             /* H;i;M|j;K;N */ &\
  nameb##3  R0, *+AR1(1), R2        &||  namea     R0, *+AR0(1), R0         /* H;i;M|K;j;N */ &\
  nameb##3  *+AR0(1), *+AR1(1), R2  &||  namea     R2, R1, R0               /* i;j;M|H;K;N */ &\
  nameb##3  *+AR0(1), *+AR1(1), R2  &||  namea     R2, R0                   /* i;j;M|H;K;N */ &\
  nameb##3  *+AR1(1), R3, R2        &||  namea     *+AR0(1), R1, R0         /* i;H;M|j;K;N */ &\
  nameb##3  *+AR1(1), R3, R2        &||  namea     *+AR0(1), R0             /* i;H;M|j;K;N */ &\
  nameb##3  *+AR1(1), R2            &||  namea     *+AR0(1), R1, R0         /* i;H;M|j;K;N */ &\
  nameb##3  *+AR1(1), R2            &||  namea     *+AR0(1), R0             /* i;H;M|j;K;N */ &\
  nameb##3  *+AR1(1), R0, R2        &||  namea     R0, *+AR0(1), R0         /* i;H;M|K;j;N */ &\
  nameb##3  *+AR1(1), R2            &||  namea     R0, *+AR0(1), R0         /* i;H;M|K;j;N */ &\
  .endif                                                                                      &\
  .ifdef TEST_ENH                                                                             &\
nameb##3_##namea##_M_enh:                                                                     &\
  nameb##3  R2, R2, R2              &||  namea     R0, R0, R0               /* H;K;M|i;j;N */ &\
  nameb##3  R2, R2, R2              &||  namea     R0, R0                   /* H;K;M|i;j;N */ &\
  nameb##3  R2, R2, R2              &||  namea     R0                       /* H;K;M|i;j;N */ &\
  nameb##3  R2, R2                  &||  namea     R0, R0                   /* H;K;M|i;j;N */ &\
  nameb##3  R2, R2                  &||  namea     R0                       /* H;K;M|i;j;N */ &\
  nameb##3  R2                      &||  namea     R0                       /* H;K;M|i;j;N */ &\
  nameb##3  R2, R2, R2              &||  namea     AR0, AR0, R0             /* H;K;M|i;j;N */ &\
  nameb##3  R0, AR0, R2             &||  namea     AR0, R0, R0              /* H;i;M|j;K;N */ &\
  nameb##3  R0, AR0, R2             &||  namea     R0, AR0, R0              /* H;i;M|K;j;N */ &\
  nameb##3  AR0, AR1, R2            &||  namea     R2, R1, R0               /* i;j;M|H;K;N */ &\
  nameb##3  AR0, R3, R2             &||  namea     AR0, R1, R0              /* i;H;M|j;K;N */ &\
  nameb##3  AR0, R0, R2             &||  namea     R0, AR0, R0              /* i;H;M|K;j;N */ &\
  .endif                                                                                      &\
  .ifdef level                                                                                &\
nameb##_##namea##3_M:                                                                         &\
  nameb     R0, R1, R2              &||  namea##3  *+AR0(1), *+AR1(1), R0   /* H;K;M|i;j;N */ &\
  nameb     R0, R2                  &||  namea##3  *+AR0(1), *+AR1(1), R0   /* H;K;M|i;j;N */ &\
  nameb     R0, *+AR1(1), R2        &||  namea##3  *+AR0(1), R0, R0         /* H;i;M|j;K;N */ &\
  nameb     R0, *+AR1(1), R2        &||  namea##3  *+AR0(1), R0             /* H;i;M|j;K;N */ &\
  nameb     R0, *+AR1(1), R2        &||  namea##3  R0, *+AR0(1), R0         /* H;i;M|K;j;N */ &\
  nameb     *+AR0(1), *+AR1(1), R2  &||  namea##3  R2, R1, R0               /* i;j;M|H;K;N */ &\
  nameb     *+AR0(1), *+AR1(1), R2  &||  namea##3  R2, R0                   /* i;j;M|H;K;N */ &\
  nameb     *+AR1(1), R3, R2        &||  namea##3  *+AR0(1), R1, R0         /* i;H;M|j;K;N */ &\
  nameb     *+AR1(1), R3, R2        &||  namea##3  *+AR0(1), R0             /* i;H;M|j;K;N */ &\
  nameb     *+AR1(1), R2            &||  namea##3  *+AR0(1), R1, R0         /* i;H;M|j;K;N */ &\
  nameb     *+AR1(1), R2            &||  namea##3  *+AR0(1), R0             /* i;H;M|j;K;N */ &\
  nameb     *+AR1(1), R0, R2        &||  namea##3  R0, *+AR0(1), R0         /* i;H;M|K;j;N */ &\
  nameb     *+AR1(1), R2            &||  namea##3  R0, *+AR0(1), R0         /* i;H;M|K;j;N */ &\
  .endif                                                                                      &\
  .ifdef TEST_ENH                                                                             &\
nameb##_##namea##3_M_enh:                                                                     &\
  nameb     R2, R2, R2              &||  namea##3  R0, R0, R0               /* H;K;M|i;j;N */ &\
  nameb     R2, R2, R2              &||  namea##3  R0, R0                   /* H;K;M|i;j;N */ &\
  nameb     R2, R2, R2              &||  namea##3  R0                       /* H;K;M|i;j;N */ &\
  nameb     R2, R2                  &||  namea##3  R0, R0                   /* H;K;M|i;j;N */ &\
  nameb     R2, R2                  &||  namea##3  R0                       /* H;K;M|i;j;N */ &\
  nameb     R2                      &||  namea##3  R0                       /* H;K;M|i;j;N */ &\
  nameb     R2, R2, R2              &||  namea##3  AR0, AR0, R0             /* H;K;M|i;j;N */ &\
  nameb     R0, AR0, R2             &||  namea##3  AR0, R0, R0              /* H;i;M|j;K;N */ &\
  nameb     R0, AR0, R2             &||  namea##3  R0, AR0, R0              /* H;i;M|K;j;N */ &\
  nameb     AR0, AR1, R2            &||  namea##3  R2, R1, R0               /* i;j;M|H;K;N */ &\
  nameb     AR0, R3, R2             &||  namea##3  AR0, R1, R0              /* i;H;M|j;K;N */ &\
  nameb     AR0, R0, R2             &||  namea##3  R0, AR0, R0              /* i;H;M|K;j;N */ &\
  .endif                                                                                      &\
  .ifdef level                                                                                &\
nameb##3_##namea##3_M:                                                                        &\
  nameb##3  R0, R1, R2              &||  namea##3  *+AR0(1), *+AR1(1), R0   /* H;K;M|i;j;N */ &\
  nameb##3  R0, R2                  &||  namea##3  *+AR0(1), *+AR1(1), R0   /* H;K;M|i;j;N */ &\
  nameb##3  R0, *+AR1(1), R2        &||  namea##3  *+AR0(1), R0, R0         /* H;i;M|j;K;N */ &\
  nameb##3  R0, *+AR1(1), R2        &||  namea##3  *+AR0(1), R0             /* H;i;M|j;K;N */ &\
  nameb##3  R0, *+AR1(1), R2        &||  namea##3  R0, *+AR0(1), R0         /* H;i;M|K;j;N */ &\
  nameb##3  *+AR0(1), *+AR1(1), R2  &||  namea##3  R2, R1, R0               /* i;j;M|H;K;N */ &\
  nameb##3  *+AR0(1), *+AR1(1), R2  &||  namea##3  R2, R0                   /* i;j;M|H;K;N */ &\
  nameb##3  *+AR1(1), R3, R2        &||  namea##3  *+AR0(1), R1, R0         /* i;H;M|j;K;N */ &\
  nameb##3  *+AR1(1), R3, R2        &||  namea##3  *+AR0(1), R0             /* i;H;M|j;K;N */ &\
  nameb##3  *+AR1(1), R2            &||  namea##3  *+AR0(1), R1, R0         /* i;H;M|j;K;N */ &\
  nameb##3  *+AR1(1), R2            &||  namea##3  *+AR0(1), R0             /* i;H;M|j;K;N */ &\
  nameb##3  *+AR1(1), R0, R2        &||  namea##3  R0, *+AR0(1), R0         /* i;H;M|K;j;N */ &\
  nameb##3  *+AR1(1), R2            &||  namea##3  R0, *+AR0(1), R0         /* i;H;M|K;j;N */ &\
  .endif                                                                                      &\
  .ifdef TEST_ENH                                                                             &\
nameb##3_##namea##3_M_enh:                                                                    &\
  nameb##3  R2, R2, R2              &||  namea##3  R0, R0, R0               /* H;K;M|i;j;N */ &\
  nameb##3  R2, R2, R2              &||  namea##3  R0, R0                   /* H;K;M|i;j;N */ &\
  nameb##3  R2, R2, R2              &||  namea##3  R0                       /* H;K;M|i;j;N */ &\
  nameb##3  R2, R2                  &||  namea##3  R0, R0                   /* H;K;M|i;j;N */ &\
  nameb##3  R2, R2                  &||  namea##3  R0                       /* H;K;M|i;j;N */ &\
  nameb##3  R2                      &||  namea##3  R0                       /* H;K;M|i;j;N */ &\
  nameb##3  R2, R2, R2              &||  namea##3  AR0, AR0, R0             /* H;K;M|i;j;N */ &\
  nameb##3  R0, AR0, R2             &||  namea##3  AR0, R0, R0              /* H;i;M|j;K;N */ &\
  nameb##3  R0, AR0, R2             &||  namea##3  R0, AR0, R0              /* H;i;M|K;j;N */ &\
  nameb##3  AR0, AR1, R2            &||  namea##3  R2, R1, R0               /* i;j;M|H;K;N */ &\
  nameb##3  AR0, R3, R2             &||  namea##3  AR0, R1, R0              /* i;H;M|j;K;N */ &\
  nameb##3  AR0, R0, R2             &||  namea##3  R0, AR0, R0              /* i;H;M|K;j;N */ &\
  .endif

/* P: General 2-operand operation with parallell store
   Syntax: <ia> src2, dst1 || <ib> src3, dst2
       src2 = Indirect 0,1,IR0,IR1, ENH: register (i)
       dst1 = Register 0-7 (L)
       src3 = Register 0-7 (H)
       dst2 = Indirect 0,1,IR0,IR1 (J)
   Instr: 9/2 - ABSF||STF, ABSI||STI, FIX||STI, FLOAT||STF, LDF||STF,
                LDI||STI, NEGF||STF, NEGI||STI, NOT||STI, C4x: FRIEEE||STF,
                TOIEEE||STF
   Alias: a||b, b||a
*/
#define P_CLASS(namea, nameb, level) \
  .ifdef level                                                  &\
namea##_##nameb##_P:                                            &\
  namea  *+AR0(1), R0  &||  nameb  R1, *+AR1(1)   /* i;L|H,J */ &\
  nameb  R1, *+AR1(1)  &||  namea  *+AR0(1), R0   /* H,J|i;L */ &\
  .endif                                                        &\
  .ifdef TEST_ENH                                               &\
namea##_##nameb##_P_enh:                                        &\
  namea  R0, R0        &||  nameb  R1, *+AR1(1)   /* i;L|H,J */ &\
  namea  R0            &||  nameb  R1, *+AR1(1)   /* i;L|H,J */ &\
  nameb  R1, *+AR1(1)  &||  namea  R0, R0         /* H,J|i;L */ &\
  nameb  R1, *+AR1(1)  &||  namea  R0             /* H,J|i;L */ &\
  .endif
  

/* Q: General 3-operand operation with parallell store
   Syntax: <ia> src1, src2, dst1 || <ib> src3, dst2
       src1 = Register 0-7 (K)
       src2 = Indirect 0,1,IR0,IR1, ENH: register (i)
       dst1 = Register 0-7 (L)
       src3 = Register 0-7 (H)
       dst2 = Indirect 0,1,IR0,IR1 (J)
   Instr: 4/0 - ASH3||STI, LSH3||STI, SUBF3||STF, SUBI3||STI
   Alias: a||b, b||a, a3||b, b||a3
*/
#define Q_CLASS(namea, nameb, level) \
  .ifdef level                                                                  &\
namea##_##nameb##_Q:                                                            &\
  namea     R0, *+AR0(1), R0  &||  nameb     R1, *+AR1(1)       /* K,i;L|H,J */ &\
  nameb     R1, *+AR1(1)      &||  namea     R0, *+AR0(1), R0   /* H,J|K,i;L */ &\
  .endif                                                                        &\
  .ifdef TEST_ENH                                                               &\
namea##_##nameb##_Q_enh:                                                        &\
  namea     R0, R0, R0        &||  nameb     R1, *+AR1(1)       /* K,i;L|H,J */ &\
  namea     R0, R0            &||  nameb     R1, *+AR1(1)       /* K,i;L|H,J */ &\
  nameb     R1, *+AR1(1)      &||  namea     R0, R0, R0         /* H,J|K,i;L */ &\
  nameb     R1, *+AR1(1)      &||  namea     R0, R0             /* H,J|K,i;L */ &\
  .endif                                                                        &\
  .ifdef level                                                                  &\
namea##3_##nameb##_Q:                                                           &\
  namea##3  R0, *+AR0(1), R0  &||  nameb     R1, *+AR1(1)       /* K,i;L|H,J */ &\
  nameb     R1, *+AR1(1)      &||  namea##3  R0, *+AR0(1), R0   /* H,J|K,i;L */ &\
  .endif                                                                        &\
  .ifdef TEST_ENH                                                               &\
namea##3_##nameb##_Q_enh:                                                       &\
  namea##3  R0, R0, R0        &||  nameb     R1, *+AR1(1)       /* K,i;L|H,J */ &\
  namea##3  R0, R0            &||  nameb     R1, *+AR1(1)       /* K,i;L|H,J */ &\
  nameb     R1, *+AR1(1)      &||  namea##3  R0, R0, R0         /* H,J|K,i;L */ &\
  nameb     R1, *+AR1(1)      &||  namea##3  R0, R0             /* H,J|K,i;L */ &\
  .endif


/* QC: General commutative 3-operand operation with parallell store
   Syntax: <ia> src2, src1, dst1 || <ib> src3, dst2
           <ia> src1, src2, dst1 || <ib> src3, dst2 - Manual
       src1 = Register 0-7 (K)
       src2 = Indirect 0,1,IR0,IR1, ENH: register (i)
       dst1 = Register 0-7 (L)
       src3 = Register 0-7 (H)
       dst2 = Indirect 0,1,IR0,IR1 (J)
   Instr: 7/0 - ADDF3||STF, ADDI3||STI, AND3||STI, MPYF3||STF, MPYI3||STI,
                OR3||STI, XOR3||STI
   Alias: a||b, b||a, a3||b, b||a3
*/
#define QC_CLASS(namea, nameb, level) \
  .ifdef level                                                                  &\
namea##_##nameb##_QC:                                                           &\
  namea     *+AR0(1), R1, R0  &||  nameb     R1, *+AR1(1)       /* i;K;L|H,J */ &\
  namea     *+AR0(1), R0      &||  nameb     R1, *+AR1(1)       /* i;K;L|H,J */ &\
  namea     R0, *+AR0(1), R0  &||  nameb     R1, *+AR1(1)       /* K;i;L|H,J */ &\
  nameb     R1, *+AR1(1)      &||  namea     *+AR0(1), R1, R0   /* H,J|i;K;L */ &\
  nameb     R1, *+AR1(1)      &||  namea     *+AR0(1), R0       /* H,J|i;K;L */ &\
  nameb     R1, *+AR1(1)      &||  namea     R0, *+AR0(1), R0   /* H,J|K;i;L */ &\
  .endif                                                                        &\
  .ifdef TEST_ENH                                                               &\
namea##_##nameb##_QC_enh:                                                       &\
  namea     AR0, R1, R0       &||  nameb     R1, *+AR1(1)       /* i;K;L|H,J */ &\
  namea     R2, R1, R0        &||  nameb     R1, *+AR1(1)       /* i;K;L|H,J */ &\
  namea     R1, R0            &||  nameb     R1, *+AR1(1)       /* i;K;L|H,J */ &\
  namea     R0                &||  nameb     R1, *+AR1(1)       /* i;K;L|H,J */ &\
  namea     R0, AR0, R0       &||  nameb     R1, *+AR1(1)       /* K;i;L|H,J */ &\
  nameb     R1, *+AR1(1)      &||  namea     AR0, R1, R0        /* H,J|i;K;L */ &\
  nameb     R1, *+AR1(1)      &||  namea     R2, R1, R0         /* H,J|i;K;L */ &\
  nameb     R1, *+AR1(1)      &||  namea     R1, R0             /* H,J|i;K;L */ &\
  nameb     R1, *+AR1(1)      &||  namea     R0                 /* H,J|i;K;L */ &\
  nameb     R1, *+AR1(1)      &||  namea     R0, AR0, R0        /* H,J|K;i;L */ &\
  .endif                                                                        &\
  .ifdef level                                                                  &\
namea##3_##nameb##_QC:                                                          &\
  namea##3  *+AR0(1), R1, R0  &||  nameb     R1, *+AR1(1)       /* i;K;L|H,J */ &\
  namea##3  *+AR0(1), R0      &||  nameb     R1, *+AR1(1)       /* i;K;L|H,J */ &\
  namea##3  R0, *+AR0(1), R0  &||  nameb     R1, *+AR1(1)       /* K;i;L|H,J */ &\
  nameb     R1, *+AR1(1)      &||  namea##3  *+AR0(1), R1, R0   /* H,J|i;K;L */ &\
  nameb     R1, *+AR1(1)      &||  namea##3  *+AR0(1), R0       /* H,J|i;K;L */ &\
  nameb     R1, *+AR1(1)      &||  namea##3  R0, *+AR0(1), R0   /* H,J|K;i;L */ &\
  .endif                                                                        &\
  .ifdef TEST_ENH                                                               &\
namea##3_##nameb##_QC_enh:                                                      &\
  namea##3  AR0, R1, R0       &||  nameb     R1, *+AR1(1)       /* i;K;L|H,J */ &\
  namea##3  R2, R1, R0        &||  nameb     R1, *+AR1(1)       /* i;K;L|H,J */ &\
  namea##3  R1, R0            &||  nameb     R1, *+AR1(1)       /* i;K;L|H,J */ &\
  namea##3  R0                &||  nameb     R1, *+AR1(1)       /* i;K;L|H,J */ &\
  namea##3  R0, AR0, R0       &||  nameb     R1, *+AR1(1)       /* K;i;L|H,J */ &\
  nameb     R1, *+AR1(1)      &||  namea##3  AR0, R1, R0        /* H,J|i;K;L */ &\
  nameb     R1, *+AR1(1)      &||  namea##3  R2, R1, R0         /* H,J|i;K;L */ &\
  nameb     R1, *+AR1(1)      &||  namea##3  R1, R0             /* H,J|i;K;L */ &\
  nameb     R1, *+AR1(1)      &||  namea##3  R0                 /* H,J|i;K;L */ &\
  nameb     R1, *+AR1(1)      &||  namea##3  R0, AR0, R0        /* H,J|K;i;L */ &\
  .endif


/* R: General register integer operation
   Syntax: <i> dst
       dst = Register (R)
   Instr: 6/0 - POP, PUSH, ROL, ROLC, ROR, RORC
*/
#define R_CLASS(name, level) \
  .ifdef level                    &\
name##_R:                         &\
  name  AR0               /* R */ &\
  .endif


/* RF: General register float operation
   Syntax: <i> dst
       dst = Register 0-11 (r)
   Instr: 2/0 - POPF, PUSHF
*/
#define RF_CLASS(name, level) \
  .ifdef level                    &\
name##_RF:                        &\
  name  F0                /* r */ &\
  .endif


/* S: General 3-operand float operation
   Syntax: <i> src2, src1, dst
       src2 = Register 0-11 (e), Indirect 0,1,IR0,IR1 (I), C4x T2: Indirect (C)
       src1 = Register 0-11 (g), Indirect 0,1,IR0,IR1 (J), C4x T2: Indirect (O)
       dst  = Register 0-11 (r)
   Instr: 1/0 - SUBF3
   Alias: i, i3
*/
#define S_CLASS(name, level) \
  .ifdef level                                  &\
name##_S:                                       &\
  name     R2, R1, R0               /* e,g;r */ &\
  name     R1, R0                   /* e,g;r */ &\
  name     R1, *+AR0(1), R0         /* e,J,r */ &\
  name     *+AR0(1), R1, R0         /* I,g;r */ &\
  name     *+AR0(1), R0             /* I,g;r */ &\
  name     *+AR0(1), *+AR1(1), R0   /* I,J,r */ &\
  .endif                                        &\
  .ifdef TEST_C4X                               &\
name##_S_c4x:                                   &\
  name     *+AR0(5), R1, R0         /* C,g;r */ &\
  name     *+AR0(5), R0             /* C,g;r */ &\
  name     *+AR0(5), *+AR1(5), R0   /* C,O,r */ &\
  .endif                                        &\
  .ifdef level                                  &\
name##3_S:                                      &\
  name##3  R2, R1, R0               /* e,g;r */ &\
  name##3  R1, R0                   /* e,g;r */ &\
  name##3  R1, *+AR0(1), R0         /* e,J,r */ &\
  name##3  *+AR0(1), R1, R0         /* I,g;r */ &\
  name##3  *+AR0(1), R0             /* I,g;r */ &\
  name##3  *+AR0(1), *+AR1(1), R0   /* I,J,r */ &\
  .endif                                        &\
  .ifdef TEST_C4X                               &\
name##3_S_c4x:                                  &\
  name##3  *+AR0(5), R1, R0         /* C,g;r */ &\
  name##3  *+AR0(5), R0             /* C,g;r */ &\
  name##3  *+AR0(5), *+AR1(5), R0   /* C,O,r */ &\
  .endif                                        


/* SC: General commutative 3-operand float operation
   Syntax: <i> src2, src1, dst - Manual
           <i> src1, src2, dst
       src2 = Register 0-11 (e), Indirect 0,1,IR0,IR1 (I), C4x T2: Indirect (C)
       src1 = Register 0-11 (g), Indirect 0,1,IR0,IR1 (J), C4x T2: Indirect (O)
       dst  = Register 0-11 (r)
   Instr: 2/0 - ADDF3, MPYF3
   Alias: i, i3
*/
#define SC_CLASS(name, level) \
  .ifdef level                                  &\
name##_SC:                                      &\
  name     R2, R1, R0               /* e,g;r */ &\
  name     R1, R0                   /* e,g;r */ &\
  name     R1, *+AR0(1), R0         /* e,J,r */ &\
  name     *+AR0(1), R1, R0         /* I,g;r */ &\
  name     *+AR0(1), R0             /* I,g;r */ &\
  name     *+AR0(1), *+AR1(1), R0   /* I,J,r */ &\
  .endif                                        &\
  .ifdef TEST_C4X                               &\
name##_SC_c4x:                                  &\
  name     *+AR0(5), R1, R0         /* C,g;r */ &\
  name     *+AR0(5), R0             /* C,g;r */ &\
  name     R1, *+AR0(5), R0         /* g,C,r */ &\
  name     *+AR0(5), *+AR1(5), R0   /* C,O,r */ &\
  .endif                                        &\
  .ifdef level                                  &\
name##3_SC:                                     &\
  name##3  R2, R1, R0               /* e,g;r */ &\
  name##3  R1, R0                   /* e,g;r */ &\
  name##3  R1, *+AR0(1), R0         /* e,J,r */ &\
  name##3  *+AR0(1), R1, R0         /* I,g;r */ &\
  name##3  *+AR0(1), R0             /* I,g;r */ &\
  name##3  *+AR0(1), *+AR1(1), R0   /* I,J,r */ &\
  .endif                                        &\
  .ifdef TEST_C4X                               &\
name##3_SC_c4x:                                 &\
  name##3  *+AR0(5), R1, R0         /* C,g;r */ &\
  name##3  *+AR0(5), R0             /* C,g;r */ &\
  name##3  R1, *+AR0(5), R0         /* g,C,r */ &\
  name##3  *+AR0(5), *+AR1(5), R0   /* C,O,r */ &\
  .endif                                        


/* S2: General 3-operand float operation with 2 args
   Syntax: <i> src2, src1
       src2 = Register 0-11 (e), Indirect 0,1,IR0,IR1 (I), C4x T2: Indirect (C)
       src1 = Register 0-11 (g), Indirect 0,1,IR0,IR1 (J), C4x T2: Indirect (O)
   Instr: 1/0 - CMPF3
   Alias: i, i3
*/
#define S2_CLASS(name, level) \
  .ifdef level                                &\
name##_S2:                                    &\
  name     R2, R1                   /* e,g */ &\
  name     R1, *+AR0(1)             /* e,J */ &\
  name     *+AR0(1), R1             /* I,g */ &\
  name     *+AR0(1), *+AR1(1)       /* I,J */ &\
  .endif                                      &\
  .ifdef TEST_C4X                             &\
name##_S2_c4x:                                &\
  name     *+AR0(5), R1             /* C,g */ &\
  name     *+AR0(5), *+AR1(5)       /* C,O */ &\
  .endif                                      &\
  .ifdef level                                &\
name##3_S2:                                   &\
  name##3  R2, R1                   /* e,g */ &\
  name##3  R1, *+AR0(1)             /* e,J */ &\
  name##3  *+AR0(1), R1             /* I,g */ &\
  name##3  *+AR0(1), *+AR1(1)       /* I,J */ &\
  .endif                                      &\
  .ifdef TEST_C4X                             &\
name##3_S2_c4x:                               &\
  name##3  *+AR0(5), R1             /* C,g */ &\
  name##3  *+AR0(5), *+AR1(5)       /* C,O */ &\
  .endif                                        


/* T: General 3-operand integer operand
   Syntax: <i> src2, src1, dst
       src2 = Register (E), Indirect 0,1,IR0,IR1 (I), C4x T2: Indirect (C), Immediate (W)
       src1 = Register (G), Indirect 0,1,IR0,IR1 (J), C4x T2: Indirect (O)
       dst  = Register (R)
   Instr: 5/0 - ANDN3, ASH3, LSH3, SUBB3, SUBI3
   Alias: i, i3
*/
#define T_CLASS(name, level) \
  .ifdef level                                   &\
name##_T:                                        &\
  name     AR2, AR1, AR0             /* E,G;R */ &\
  name     AR1, AR0                  /* E,G;R */ &\
  name     AR1, *+AR0(1), AR0        /* E,J,R */ &\
  name     *+AR0(1), AR1, AR0        /* I,G;R */ &\
  name     *+AR0(1), AR0             /* I,G;R */ &\
  name     *+AR1(1), *+AR0(1), AR0   /* I,J,R */ &\
  .endif                                         &\
  .ifdef   TEST_C4X                              &\
name##_T_sc:                                     &\
  name     -5, AR1, AR0              /* W,G;R */ &\
  name     -5, AR0                   /* W,G;R */ &\
  name     *+AR0(5), AR1, AR0        /* C,G;R */ &\
  name     *+AR0(5), AR0             /* C,G;R */ &\
  name     -5, *+AR0(5), AR0         /* W,O,R */ &\
  name     *+AR0(5), *+AR1(5), AR0   /* C,O,R */ &\
  .endif                                         &\
  .ifdef level                                   &\
name##3_T:                                       &\
  name##3  AR2, AR1, AR0             /* E,G;R */ &\
  name##3  AR1, AR0                  /* E,G;R */ &\
  name##3  AR1, *+AR0(1), AR0        /* E,J,R */ &\
  name##3  *+AR0(1), AR1, AR0        /* I,G;R */ &\
  name##3  *+AR0(1), AR0             /* I,G;R */ &\
  name##3  *+AR1(1), *+AR0(1), AR0   /* I,J,R */ &\
  .endif                                         &\
  .ifdef   TEST_C4X                              &\
name##3_T_sc:                                    &\
  name##3  -5, AR1, AR0              /* W,G;R */ &\
  name##3  -5, AR0                   /* W,G;R */ &\
  name##3  *+AR0(5), AR1, AR0        /* C,G;R */ &\
  name##3  *+AR0(5), AR0             /* C,G;R */ &\
  name##3  -5, *+AR0(5), AR0         /* W,O,R */ &\
  name##3  *+AR0(5), *+AR1(5), AR0   /* C,O,R */ &\
  .endif


/* TC: General commutative 3-operand integer operation
   Syntax: <i> src2, src1, dst
           <i> src1, src2, dst
       src2 = Register (E), Indirect 0,1,IR0,IR1 (I), C4x T2: Indirect (C), Immediate (W)
       src1 = Register (G), Indirect 0,1,IR0,IR1 (J), C4x T2: Indirect (O)
       dst  = Register (R)
   Instr: 6/2 - ADDC3, ADDI3, AND3, MPYI3, OR3, XOR3, C4x: MPYSHI, MPYUHI
   Alias: i, i3
*/
#define TC_CLASS(name, level) \
  .ifdef level                                   &\
name##_TC:                                       &\
  name     AR2, AR1, AR0             /* E,G;R */ &\
  name     AR1, AR0                  /* E,G;R */ &\
  name     AR1, *+AR0(1), AR0        /* E,J,R */ &\
  name     *+AR0(1), AR1, AR0        /* I,G;R */ &\
  name     *+AR0(1), AR0             /* I,G;R */ &\
  name     *+AR1(1), *+AR0(1), AR0   /* I,J,R */ &\
  .endif                                         &\
  .ifdef   TEST_C4X                              &\
name##_TC_c4x:                                   &\
  name     -5, AR1, AR0              /* W,G;R */ &\
  name     -5, AR0                   /* W,G;R */ &\
  name     AR1, -5, AR0              /* G,W,R */ &\
  name     *+AR0(5), AR1, AR0        /* C,G;R */ &\
  name     *+AR0(5), AR0             /* C,G;R */ &\
  name     AR1, *+AR0(5), AR0        /* G,C,R */ &\
  name     -5, *+AR0(5), AR0         /* W,O,R */ &\
  name     *+AR0(5), -5, AR0         /* O,W,R */ &\
  name     *+AR0(5), *+AR1(5), AR0   /* C,O,R */ &\
  .endif                                         &\
  .ifdef level                                   &\
name##3_TC:                                      &\
  name##3  AR2, AR1, AR0             /* E,G;R */ &\
  name##3  AR1, AR0                  /* E,G;R */ &\
  name##3  AR1, *+AR0(1), AR0        /* E,J,R */ &\
  name##3  *+AR0(1), AR1, AR0        /* I,G;R */ &\
  name##3  *+AR0(1), AR0             /* I,G;R */ &\
  name##3  *+AR1(1), *+AR0(1), AR0   /* I,J,R */ &\
  .endif                                         &\
  .ifdef   TEST_C4X                              &\
name##3_TC_c4x:                                  &\
  name##3  -5, AR1, AR0              /* W,G;R */ &\
  name##3  -5, AR0                   /* W,G;R */ &\
  name##3  AR1, -5, AR0              /* G,W,R */ &\
  name##3  *+AR0(5), AR1, AR0        /* C,G;R */ &\
  name##3  *+AR0(5), AR0             /* C,G;R */ &\
  name##3  AR1, *+AR0(5), AR0        /* G,C,R */ &\
  name##3  -5, *+AR0(5), AR0         /* W,O,R */ &\
  name##3  *+AR0(5), -5, AR0         /* O,W,R */ &\
  name##3  *+AR0(5), *+AR1(5), AR0   /* C,O,R */ &\
  .endif


/* T2: General 3-operand integer operation with 2 args
   Syntax: <i> src2, src1
       src2 = Register (E), Indirect 0,1,IR0,IR1 (I), C4x T2: Indirect (C), Immediate (W)
       src1 = Register (G), Indirect 0,1,IR0,IR1 (J), C4x T2: Indirect (O)
   Instr: 1/0 - CMPI3
   Alias: i, i3
*/
#define T2_CLASS(name, level) \
  .ifdef level                                 &\
name##_T2:                                     &\
  name     AR2, AR1                  /* E,G */ &\
  name     AR1, *+AR0(1)             /* E,J */ &\
  name     *+AR0(1), AR1             /* I,G */ &\
  name     *+AR1(1), *+AR0(1)        /* I,J */ &\
  .endif                                       &\
  .ifdef   TEST_C4X                            &\
name##_T2_c4x:                                 &\
  name     -5, AR1                   /* W,G */ &\
  name     *+AR0(5), AR1             /* C,G */ &\
  name     -5, *+AR0(5)              /* W,O */ &\
  name     *+AR0(5), *+AR1(5)        /* C,O */ &\
  .endif                                       &\
  .ifdef level                                 &\
name##3_T2:                                    &\
  name##3  AR2, AR1                  /* E,G */ &\
  name##3  AR1, *+AR0(1)             /* E,J */ &\
  name##3  *+AR0(1), AR1             /* I,G */ &\
  name##3  *+AR1(1), *+AR0(1)        /* I,J */ &\
  .endif                                       &\
  .ifdef   TEST_C4X                            &\
name##3_T2_c4x:                                &\
  name##3  -5, AR1                   /* W,G */ &\
  name##3  *+AR0(5), AR1             /* C,G */ &\
  name##3  -5, *+AR0(5)              /* W,O */ &\
  name##3  *+AR0(5), *+AR1(5)        /* C,O */ &\
  .endif


/* T2C: General commutative 3-operand integer operation with 2 args 
   Syntax: <i> src2, src1 - Manual
           <i> src1, src2 
       src2 = Register (E), Indirect 0,1,IR0,IR1 (I), C4x T2: Indirect (C), Immediate (W)
       src1 = Register (G), Indirect 0,1,IR0,IR1 (J), C4x T2: Indirect (0)
   Instr: 1/0 - TSTB3
   Alias: i, i3
*/
#define T2C_CLASS(name, level) \
  .ifdef level                                 &\
name##_T2C:                                    &\
  name     AR2, AR1                  /* E,G */ &\
  name     AR1, *+AR0(1)             /* E,J */ &\
  name     *+AR0(1), AR1             /* I,G */ &\
  name     *+AR1(1), *+AR0(1)        /* I,J */ &\
  .endif                                       &\
  .ifdef   TEST_C4X                            &\
name##_T2C_c4x:                                &\
  name     -5, AR1                   /* W,G */ &\
  name     AR1, -5                   /* G,W */ &\
  name     *+AR0(5), AR1             /* C,G */ &\
  name     AR1, *+AR0(5)             /* G,C */ &\
  name     -5, *+AR0(5)              /* W,O */ &\
  name     *+AR0(5), -5              /* O,W */ &\
  name     *+AR0(5), *+AR1(5)        /* C,O */ &\
  .endif                                       &\
  .ifdef level                                 &\
name##3_T2C:                                   &\
  name##3  AR2, AR1                  /* E,G */ &\
  name##3  AR1, *+AR0(1)             /* E,J */ &\
  name##3  *+AR0(1), AR1             /* I,G */ &\
  name##3  *+AR1(1), *+AR0(1)        /* I,J */ &\
  .endif                                       &\
  .ifdef   TEST_C4X                            &\
name##3_T2C_c4x:                               &\
  name##3  -5, AR1                   /* W,G */ &\
  name##3  AR1, -5                   /* G,W */ &\
  name##3  *+AR0(5), AR1             /* C,G */ &\
  name##3  AR1, *+AR0(5)             /* G,C */ &\
  name##3  -5, *+AR0(5)              /* W,O */ &\
  name##3  *+AR0(5), -5              /* O,W */ &\
  name##3  *+AR0(5), *+AR1(5)        /* C,O */ &\
  .endif
