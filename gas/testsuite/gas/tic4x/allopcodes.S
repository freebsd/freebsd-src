;;; 
;;;  Test all opcodes and argument permuation
;;;  To make our job a lot simpler, we define a couple of
;;;  insn classes, that we use to generate the proper
;;;  test output.
;;;  
;;;  To rebuild this file you must use
;;;    ./rebuild.sh
;;;
;;;  These definitions are used within this file:
;;;    TEST_C3X    Enables testing of c3x opcodes
;;;    TEST_C4X    Enables testing of c4x opcodes
;;;    TEST_ENH    Enable testing of enhanced opcodes
;;;    TEST_IDLE2  Enable testing of IDLE2 command
;;;    TEST_LPWR   Enable testing of LOPOWER commands
;;; 
#include "opclasses.h"
        
            .text
            ;;------------------------------------
            ;;  C3X INSNS
            ;;------------------------------------ 
start:      B_CLASS(   absf,        TEST_C3X )
            P_CLASS(   absf, stf,   TEST_C3X )
            A_CLASS(   absi,        TEST_C3X )
            P_CLASS(   absi, sti,   TEST_C3X )
            A_CLASS(   addc,        TEST_C3X )
            TC_CLASS(  addc,        TEST_C3X )
            B_CLASS(   addf,        TEST_C3X )
            SC_CLASS(  addf,        TEST_C3X )
            QC_CLASS(  addf, stf,   TEST_C3X )
            A_CLASS(   addi,        TEST_C3X )
            TC_CLASS(  addi,        TEST_C3X )
            QC_CLASS(  addi, sti,   TEST_C3X )
            AU_CLASS(  and,         TEST_C3X )
            TC_CLASS(  and,         TEST_C3X )
            QC_CLASS(  and, sti,    TEST_C3X )
            AU_CLASS(  andn,        TEST_C3X )
            T_CLASS(   andn,        TEST_C3X )
            A_CLASS(   ash,         TEST_C3X )
            T_CLASS(   ash,         TEST_C3X )
            Q_CLASS(   ash, sti,    TEST_C3X )
            J_CLASS(   bC, b,       TEST_C3X )
            J_CLASS(   bCd, bd,     TEST_C3X )
            .ifdef TEST_C3X
br_I:       br      start
brd_I:      brd     start
call_I:     call    start
call_JS:    callc   R0
            callc   start
            .endif
            B_CLASS(   cmpf,        TEST_C3X )
            S2_CLASS(  cmpf,        TEST_C3X )
            A_CLASS(   cmpi,        TEST_C3X )
            T2_CLASS(  cmpi,        TEST_C3X )
            D_CLASS(   dbC, db,     TEST_C3X )
            D_CLASS(   dbCd, dbd,   TEST_C3X )
            AF_CLASS(  fix,         TEST_C3X )
            P_CLASS(   fix, sti,    TEST_C3X )
            BI_CLASS(  float,       TEST_C3X )
            P_CLASS(   float, stf,  TEST_C3X )
            .ifdef TEST_C3X
iack_Z:     iack    @start
            iack    *+AR0(1)
idle_Z:     idle
            .endif
            .ifdef TEST_IDLE2
idle2_Z:    idle2
            .endif
            B_CLASS(   lde,         TEST_C3X )
            B_CLASS(   ldf,         TEST_C3X )
            LL_CLASS(  ldf,         TEST_C3X )
            P_CLASS(   ldf, stf,    TEST_C3X )
            BB_CLASS(  ldfC,        TEST_C3X )
            B6_CLASS(  ldfi,        TEST_C3X )
            A_CLASS(   ldi,         TEST_C3X )
            LL_CLASS(  ldi,         TEST_C3X )
            P_CLASS(   ldi, sti,    TEST_C3X )
            AB_CLASS(  ldiC,        TEST_C3X )
            A6_CLASS(  ldii,        TEST_C3X )
            .ifdef TEST_C3X
ldp_Z:      ldp     start
            .endif
            B_CLASS(  ldm,          TEST_C3X )
            .ifdef TEST_LPWR
lopower_Z:  lopower
            .endif
            A_CLASS(   lsh,         TEST_C3X )
            T_CLASS(   lsh,         TEST_C3X )
            Q_CLASS(   lsh, sti,    TEST_C3X )
            .ifdef TEST_LPWR
maxspeed_Z: maxspeed
            .endif
            B_CLASS(   mpyf,        TEST_C3X )
            SC_CLASS(  mpyf,        TEST_C3X )
            M_CLASS(   mpyf, addf,  TEST_C3X )
            QC_CLASS(  mpyf, stf,   TEST_C3X )
            M_CLASS(   mpyf, subf,  TEST_C3X )
            A_CLASS(   mpyi,        TEST_C3X )
            TC_CLASS(  mpyi,        TEST_C3X )
            M_CLASS(   mpyi, addi,  TEST_C3X )
            QC_CLASS(  mpyi, sti,   TEST_C3X )
            M_CLASS(   mpyi, subi,  TEST_C3X )
            A_CLASS(   negb,        TEST_C3X )
            B_CLASS(   negf,        TEST_C3X )
            P_CLASS(   negf, stf,   TEST_C3X )
            A_CLASS(   negi,        TEST_C3X )
            P_CLASS(   negi, sti,   TEST_C3X )
            A2_CLASS(  nop,         TEST_C3X )
            B_CLASS(   norm,        TEST_C3X )
            AU_CLASS(  not,         TEST_C3X )
            P_CLASS(   not, sti,    TEST_C3X )
            AU_CLASS(  or,          TEST_C3X )
            TC_CLASS(  or,          TEST_C3X )
            QC_CLASS(  or, sti,     TEST_C3X )
            R_CLASS(   pop,         TEST_C3X )
            RF_CLASS(  popf,        TEST_C3X )
            R_CLASS(   push,        TEST_C3X )
            RF_CLASS(  pushf,       TEST_C3X )
            .ifdef TEST_C3X
reti_Z:     retiC
            reti
rets_Z:     retsC
            rets
            .endif
            B_CLASS(   rnd,         TEST_C3X )
            R_CLASS(   rol,         TEST_C3X )
            R_CLASS(   rolc,        TEST_C3X )
            R_CLASS(   ror,         TEST_C3X )
            R_CLASS(   rorc,        TEST_C3X )
            .ifdef TEST_C3X
rptb_I2:    rptb    start
            .endif
            A3_CLASS(  rpts,        TEST_C3X )
            .ifdef TEST_C3X
sigi_Z:     sigi
            .endif
            B7_CLASS(  stf,         TEST_C3X )
            LS_CLASS(  stf,         TEST_C3X )
            B7_CLASS(  stfi,        TEST_C3X )
            A7_CLASS(  sti,         TEST_C3X )
            LS_CLASS(  sti,         TEST_C3X )
            A7_CLASS(  stii,        TEST_C3X )
            A_CLASS(   subb,        TEST_C3X )
            T_CLASS(   subb,        TEST_C3X )
            A_CLASS(   subc,        TEST_C3X )
            B_CLASS(   subf,        TEST_C3X )
            S_CLASS(   subf,        TEST_C3X )
            Q_CLASS(   subf, stf,   TEST_C3X )
            A_CLASS(   subi,        TEST_C3X )
            T_CLASS(   subi,        TEST_C3X )
            Q_CLASS(   subi, sti,   TEST_C3X )
            A_CLASS(   subrb,       TEST_C3X )
            B_CLASS(   subrf,       TEST_C3X )
            A_CLASS(   subri,       TEST_C3X )
            .ifdef TEST_C3X
swi_Z:      swi
trap_Z:     trapC   10
            trap    10
            .endif
            AU_CLASS(  tstb,        TEST_C3X )
            T2C_CLASS( tstb,        TEST_C3X )
            AU_CLASS(  xor,         TEST_C3X )
            TC_CLASS(  xor,         TEST_C3X )
            QC_CLASS(  xor, sti,    TEST_C3X )

            ;;------------------------------------
            ;;  C4X INSNS
            ;;------------------------------------ 
            .ifdef  TEST_C4X
            J_CLASS(   bCaf, baf,   TEST_C4X )
            J_CLASS(   bCat, bat,   TEST_C4X )
            B6_CLASS(  frieee,      TEST_C4X )
            P_CLASS(   frieee, stf, TEST_C4X )
            .ifdef TEST_C4X
laj_I:      laj     start
laj_JS:     lajc    R0
            lajc    start
lat_Z:      latC    10
            .endif
            A_CLASS(   lb0,         TEST_C4X )
            A_CLASS(   lb1,         TEST_C4X )
            A_CLASS(   lb2,         TEST_C4X )
            A_CLASS(   lb3,         TEST_C4X )
            AU_CLASS(  lbu0,        TEST_C4X )
            AU_CLASS(  lbu1,        TEST_C4X )
            AU_CLASS(  lbu2,        TEST_C4X )
            AU_CLASS(  lbu3,        TEST_C4X )
            AY_CLASS(  lda,         TEST_C4X )
            .ifdef TEST_C4X
ldep_Z:     ldep    IVTP, AR0
ldhi_Z:     ldhi    35, R0
            ldhi    start, R0
ldpe_Z:     ldpe    AR0, IVTP
ldpk_Z:     ldpk    start
            .endif
            A_CLASS(   lh0,         TEST_C4X )
            A_CLASS(   lh1,         TEST_C4X )
            AU_CLASS(  lhu0,        TEST_C4X )
            AU_CLASS(  lhu1,        TEST_C4X )
            A_CLASS(   lwl0,        TEST_C4X )
            A_CLASS(   lwl1,        TEST_C4X )
            A_CLASS(   lwl2,        TEST_C4X )
            A_CLASS(   lwl3,        TEST_C4X )
            A_CLASS(   lwr0,        TEST_C4X )
            A_CLASS(   lwr1,        TEST_C4X )
            A_CLASS(   lwr2,        TEST_C4X )
            A_CLASS(   lwr3,        TEST_C4X )
            A_CLASS(   mb0,         TEST_C4X )
            A_CLASS(   mb1,         TEST_C4X )
            A_CLASS(   mb2,         TEST_C4X )
            A_CLASS(   mb3,         TEST_C4X )
            A_CLASS(   mh0,         TEST_C4X )
            A_CLASS(   mh1,         TEST_C4X )
            A_CLASS(   mh2,         TEST_C4X )
            A_CLASS(   mh3,         TEST_C4X )
            A_CLASS(   mpyshi,      TEST_C4X )
            TC_CLASS(  mpyshi,      TEST_C4X )
            A_CLASS(   mpyuhi,      TEST_C4X )
            TC_CLASS(  mpyuhi,      TEST_C4X )
            BA_CLASS(  rcpf,        TEST_C4X )
            .ifdef TEST_C4X
retid_Z:    retiCd
            retid
rptb2_I2:   rptb    AR0
rptbd_I2:   rptbd  start
            rptbd   AR0
            .endif
            B_CLASS(   rsqrf,       TEST_C4X )
            A6_CLASS(  sigi,        TEST_C4X )
            .ifdef TEST_C4X
sti2_A7:    sti     -5, @start
            sti     -5, *+AR0(5)
stik_Z:     stik    -5, @start
            stik    -5, *+AR0(5)
            .endif
            B_CLASS(   toieee,      TEST_C4X )
            P_CLASS(   toieee, stf, TEST_C4X )
            .endif
            .end
        
