
.EXTERN MY_LABEL2;
.section .text;

//
//14 VECTOR OPERATIONS
//

//Dreg_hi = Dreg_lo = SIGN ( Dreg_hi ) * Dreg_hi + SIGN ( Dreg_lo ) * Dreg_lo ; /* (b) */

r7.h=r7.l=sign(r2.h)*r3.h+sign(r2.l)*r3.l ;
r0.h=r0.l=sign(r1.h)*r2.h+sign(r1.l)*r2.l ;
r3.h=r3.l=sign(r4.h)*r5.h+sign(r4.l)*r5.l ;
r6.h=r6.l=sign(r7.h)*r0.h+sign(r7.l)*r0.l ;
r1.h=r1.l=sign(r2.h)*r3.h+sign(r2.l)*r3.l ;
r4.h=r4.l=sign(r5.h)*r6.h+sign(r5.l)*r6.l ;
r7.h=r7.l=sign(r0.h)*r1.h+sign(r0.l)*r1.l ;
r2.h=r2.l=sign(r3.h)*r4.h+sign(r3.l)*r4.l ;

//Dual 16-Bit Operation
//Dreg = VIT_MAX ( Dreg , Dreg ) (ASL) ; /* shift history bits left (b) */
//Dreg = VIT_MAX ( Dreg , Dreg ) (ASR) ; /* shift history bits right (b) */
//Single 16-Bit Operation
//Dreg_lo = VIT_MAX ( Dreg ) (ASL) ; /* shift history bits left (b) */
//Dreg_lo = VIT_MAX ( Dreg ) (ASR) ; /* shift history bits right (b) */
r5 = vit_max(r3, r2)(asl) ; /* shift left, dual operation */
r7 = vit_max (r1, r0) (asr) ; /* shift right, dual operation */

r0 = vit_max(r1, r2)(asl) ; /* shift left, dual operation */
r3 = vit_max (r4, r5) (asr) ; /* shift right, dual operation */
r6 = vit_max(r7, r0)(asl) ; /* shift left, dual operation */
r1 = vit_max (r2, r3) (asr) ; /* shift right, dual operation */
r4 = vit_max(r5, r6)(asl) ; /* shift left, dual operation */
r7 = vit_max (r0, r1) (asr) ; /* shift right, dual operation */
r2 = vit_max(r3, r4)(asl) ; /* shift left, dual operation */
r5 = vit_max (r6, r7) (asr) ; /* shift right, dual operation */


r3.l = vit_max (r1)(asl) ; /* shift left, single operation */
r3.l = vit_max (r1)(asr) ; /* shift right, single operation */

r0.l = vit_max (r1)(asl) ; /* shift left, single operation */
r2.l = vit_max (r3)(asr) ; /* shift right, single operation */
r4.l = vit_max (r5)(asl) ; /* shift left, single operation */
r6.l = vit_max (r7)(asr) ; /* shift right, single operation */
r1.l = vit_max (r2)(asl) ; /* shift left, single operation */
r3.l = vit_max (r4)(asr) ; /* shift right, single operation */
r5.l = vit_max (r6)(asl) ; /* shift left, single operation */
r7.l = vit_max (r0)(asr) ; /* shift right, single operation */

//Dreg = ABS Dreg (V) ; /* (b) */
r3 = abs r1 (v) ;

r0 = abs r0 (v) ;
r0 = abs r1 (v) ;
r2 = abs r3 (v) ;
r4 = abs r5 (v) ;
r6 = abs r7 (v) ;
r1 = abs r0 (v) ;
r3 = abs r2 (v) ;
r5 = abs r4 (v) ;
r7 = abs r6 (v) ;

//Dual 16-Bit Operations
//Dreg = Dreg +|+ Dreg (opt_mode_0) ; /* add | add (b) */
r5=r3 +|+ r4 ; /* dual 16-bit operations, add|add */

r0=r1 +|+ r2 ;
r3=r4 +|+ r5 ;
r6=r7 +|+ r0 ;
r1=r2 +|+ r3 ;
r4=r3 +|+ r5 ;
r6=r3 +|+ r7 ;

r0=r1 +|+ r2 (S);
r3=r4 +|+ r5 (S);
r6=r7 +|+ r0 (S);
r1=r2 +|+ r3 (S);
r4=r3 +|+ r5 (S);
r6=r3 +|+ r7 (S);

r0=r1 +|+ r2 (CO);
r3=r4 +|+ r5 (CO);
r6=r7 +|+ r0 (CO) ;
r1=r2 +|+ r3 (CO);
r4=r3 +|+ r5 (CO);
r6=r3 +|+ r7 (CO);

r0=r1 +|+ r2 (SCO);
r3=r4 +|+ r5 (SCO);
r6=r7 +|+ r0 (SCO);
r1=r2 +|+ r3 (SCO);
r4=r3 +|+ r5 (SCO);
r6=r3 +|+ r7 (SCO);

//Dreg = Dreg –|+ Dreg (opt_mode_0) ; /* subtract | add (b) */
r6=r0 -|+ r1(s) ; /* same as above, subtract|add with saturation */

r0=r1 -|+ r2 ;
r3=r4 -|+ r5 ;
r6=r7 -|+ r0 ;
r1=r2 -|+ r3 ;
r4=r3 -|+ r5 ;
r6=r3 -|+ r7 ;

r0=r1 -|+ r2 (S);
r3=r4 -|+ r5 (S);
r6=r7 -|+ r0 (S);
r1=r2 -|+ r3 (S);
r4=r3 -|+ r5 (S);
r6=r3 -|+ r7 (S);

r0=r1 -|+ r2 (CO);
r3=r4 -|+ r5 (CO);
r6=r7 -|+ r0 (CO) ;
r1=r2 -|+ r3 (CO);
r4=r3 -|+ r5 (CO);
r6=r3 -|+ r7 (CO);

r0=r1 -|+ r2 (SCO);
r3=r4 -|+ r5 (SCO);
r6=r7 -|+ r0 (SCO);
r1=r2 -|+ r3 (SCO);
r4=r3 -|+ r5 (SCO);
r6=r3 -|+ r7 (SCO);


//Dreg = Dreg +|– Dreg (opt_mode_0) ; /* add | subtract (b) */
r0=r2 +|- r1(co) ; /* add|subtract with half-word results crossed over in the destination register */

r0=r1 +|- r2 ;
r3=r4 +|- r5 ;
r6=r7 +|- r0 ;
r1=r2 +|- r3 ;
r4=r3 +|- r5 ;
r6=r3 +|- r7 ;

r0=r1 +|- r2 (S);
r3=r4 +|- r5 (S);
r6=r7 +|- r0 (S);
r1=r2 +|- r3 (S);
r4=r3 +|- r5 (S);
r6=r3 +|- r7 (S);

r0=r1 +|- r2 (CO);
r3=r4 +|- r5 (CO);
r6=r7 +|- r0 (CO) ;
r1=r2 +|- r3 (CO);
r4=r3 +|- r5 (CO);
r6=r3 +|- r7 (CO);

r0=r1 +|- r2 (SCO);
r3=r4 +|- r5 (SCO);
r6=r7 +|- r0 (SCO);
r1=r2 +|- r3 (SCO);
r4=r3 +|- r5 (SCO);
r6=r3 +|- r7 (SCO);

//Dreg = Dreg –|– Dreg (opt_mode_0) ; /* subtract | subtract (b) */
r7=r3 -|- r6(sco) ; /* subtract|subtract with saturation and half-word results crossed over in the destination register */

r0=r1 -|- r2 ;
r3=r4 -|- r5 ;
r6=r7 -|- r0 ;
r1=r2 -|- r3 ;
r4=r3 -|- r5 ;
r6=r3 -|- r7 ;

r0=r1 -|- r2 (S);
r3=r4 -|- r5 (S);
r6=r7 -|- r0 (S);
r1=r2 -|- r3 (S);
r4=r3 -|- r5 (S);
r6=r3 -|- r7 (S);

r0=r1 -|- r2 (CO);
r3=r4 -|- r5 (CO);
r6=r7 -|- r0 (CO) ;
r1=r2 -|- r3 (CO);
r4=r3 -|- r5 (CO);
r6=r3 -|- r7 (CO);

r0=r1 -|- r2 (SCO);
r3=r4 -|- r5 (SCO);
r6=r7 -|- r0 (SCO);
r1=r2 -|- r3 (SCO);
r4=r3 -|- r5 (SCO);
r6=r3 -|- r7 (SCO);

//Quad 16-Bit Operations
//Dreg = Dreg +|+ Dreg, Dreg = Dreg –|– Dreg (opt_mode_0,opt_mode_2) ; /* add | add, subtract | subtract; the set of source registers must be the same for each operation (b) */
r5=r3 +|+ r4, r7=r3-|-r4 ; /* quad 16-bit operations, add|add, subtract|subtract */

r0=r1 +|+ r2, r7=r1 -|- r2;
r3=r4 +|+ r5, r6=r4 -|- r5;
r6=r7 +|+ r0, r5=r7 -|- r0;
r1=r2 +|+ r3, r4=r2 -|- r3;
r4=r3 +|+ r5, r3=r3 -|- r5;
r6=r3 +|+ r7, r2=r3 -|- r7;
                         
r0=r1 +|+ r2, r7=r1 -|- r2(S);
r3=r4 +|+ r5, r6=r4 -|- r5(S);
r6=r7 +|+ r0, r5=r7 -|- r0(S);
r1=r2 +|+ r3, r4=r2 -|- r3(S);
r4=r3 +|+ r5, r3=r3 -|- r5(S);
r6=r3 +|+ r7, r2=r3 -|- r7(S);
                         

r0=r1 +|+ r2, r7=r1 -|- r2(CO);
r3=r4 +|+ r5, r6=r4 -|- r5(CO);
r6=r7 +|+ r0, r5=r7 -|- r0(CO);
r1=r2 +|+ r3, r4=r2 -|- r3(CO);
r4=r3 +|+ r5, r3=r3 -|- r5(CO);
r6=r3 +|+ r7, r2=r3 -|- r7(CO);
                         

r0=r1 +|+ r2, r7=r1 -|- r2(SCO);
r3=r4 +|+ r5, r6=r4 -|- r5(SCO);
r6=r7 +|+ r0, r5=r7 -|- r0(SCO);
r1=r2 +|+ r3, r4=r2 -|- r3(SCO);
r4=r3 +|+ r5, r3=r3 -|- r5(SCO);
r6=r3 +|+ r7, r2=r3 -|- r7(SCO);
                         
r0=r1 +|+ r2, r7=r1 -|- r2(ASR);
r3=r4 +|+ r5, r6=r4 -|- r5(ASR);
r6=r7 +|+ r0, r5=r7 -|- r0(ASR);
r1=r2 +|+ r3, r4=r2 -|- r3(ASR);
r4=r3 +|+ r5, r3=r3 -|- r5(ASR);
r6=r3 +|+ r7, r2=r3 -|- r7(ASR);
                         

r0=r1 +|+ r2, r7=r1 -|- r2(ASL);
r3=r4 +|+ r5, r6=r4 -|- r5(ASL);
r6=r7 +|+ r0, r5=r7 -|- r0(ASL);
r1=r2 +|+ r3, r4=r2 -|- r3(ASL);
r4=r3 +|+ r5, r3=r3 -|- r5(ASL);
r6=r3 +|+ r7, r2=r3 -|- r7(ASL);
                         

r0=r1 +|+ r2, r7=r1 -|- r2(S,ASR);
r3=r4 +|+ r5, r6=r4 -|- r5(S,ASR);
r6=r7 +|+ r0, r5=r7 -|- r0(S,ASR);
r1=r2 +|+ r3, r4=r2 -|- r3(S,ASR);
r4=r3 +|+ r5, r3=r3 -|- r5(S,ASR);
r6=r3 +|+ r7, r2=r3 -|- r7(S,ASR);
                         

r0=r1 +|+ r2, r7=r1 -|- r2(CO,ASR);
r3=r4 +|+ r5, r6=r4 -|- r5(CO,ASR);
r6=r7 +|+ r0, r5=r7 -|- r0(CO,ASR);
r1=r2 +|+ r3, r4=r2 -|- r3(CO,ASR);
r4=r3 +|+ r5, r3=r3 -|- r5(CO,ASR);
r6=r3 +|+ r7, r2=r3 -|- r7(CO,ASR);
                         

r0=r1 +|+ r2, r7=r1 -|- r2(SCO,ASR);
r3=r4 +|+ r5, r6=r4 -|- r5(SCO,ASR);
r6=r7 +|+ r0, r5=r7 -|- r0(SCO,ASR);
r1=r2 +|+ r3, r4=r2 -|- r3(SCO,ASR);
r4=r3 +|+ r5, r3=r3 -|- r5(SCO,ASR);
r6=r3 +|+ r7, r2=r3 -|- r7(SCO,ASR);

r0=r1 +|+ r2, r7=r1 -|- r2(S,ASL);
r3=r4 +|+ r5, r6=r4 -|- r5(S,ASL);
r6=r7 +|+ r0, r5=r7 -|- r0(S,ASL);
r1=r2 +|+ r3, r4=r2 -|- r3(S,ASL);
r4=r3 +|+ r5, r3=r3 -|- r5(S,ASL);
r6=r3 +|+ r7, r2=r3 -|- r7(S,ASL);
                         

r0=r1 +|+ r2, r7=r1 -|- r2(CO,ASL);
r3=r4 +|+ r5, r6=r4 -|- r5(CO,ASL);
r6=r7 +|+ r0, r5=r7 -|- r0(CO,ASL);
r1=r2 +|+ r3, r4=r2 -|- r3(CO,ASL);
r4=r3 +|+ r5, r3=r3 -|- r5(CO,ASL);
r6=r3 +|+ r7, r2=r3 -|- r7(CO,ASL);
                         

r0=r1 +|+ r2, r7=r1 -|- r2(SCO,ASL);
r3=r4 +|+ r5, r6=r4 -|- r5(SCO,ASL);
r6=r7 +|+ r0, r5=r7 -|- r0(SCO,ASL);
r1=r2 +|+ r3, r4=r2 -|- r3(SCO,ASL);
r4=r3 +|+ r5, r3=r3 -|- r5(SCO,ASL);
r6=r3 +|+ r7, r2=r3 -|- r7(SCO,ASL);


//Dreg = Dreg +|– Dreg, Dreg = Dreg –|+ Dreg (opt_mode_0,opt_mode_2) ; /* add | subtract, subtract | add; the set of source registers must be the same for each operation (b) */
r5=r3 +|- r4, r7=r3 -|+ r4 ; /* quad 16-bit operations, add|subtract, subtract|add */

r0=r1 +|- r2, r7=r1 -|+ r2;
r3=r4 +|- r5, r6=r4 -|+ r5;
r6=r7 +|- r0, r5=r7 -|+ r0;
r1=r2 +|- r3, r4=r2 -|+ r3;
r4=r3 +|- r5, r3=r3 -|+ r5;
r6=r3 +|- r7, r2=r3 -|+ r7;
                         
r0=r1 +|- r2, r7=r1 -|+ r2(S);
r3=r4 +|- r5, r6=r4 -|+ r5(S);
r6=r7 +|- r0, r5=r7 -|+ r0(S);
r1=r2 +|- r3, r4=r2 -|+ r3(S);
r4=r3 +|- r5, r3=r3 -|+ r5(S);
r6=r3 +|- r7, r2=r3 -|+ r7(S);
                         

r0=r1 +|- r2, r7=r1 -|+ r2(CO);
r3=r4 +|- r5, r6=r4 -|+ r5(CO);
r6=r7 +|- r0, r5=r7 -|+ r0(CO);
r1=r2 +|- r3, r4=r2 -|+ r3(CO);
r4=r3 +|- r5, r3=r3 -|+ r5(CO);
r6=r3 +|- r7, r2=r3 -|+ r7(CO);
                         

r0=r1 +|- r2, r7=r1 -|+ r2(SCO);
r3=r4 +|- r5, r6=r4 -|+ r5(SCO);
r6=r7 +|- r0, r5=r7 -|+ r0(SCO);
r1=r2 +|- r3, r4=r2 -|+ r3(SCO);
r4=r3 +|- r5, r3=r3 -|+ r5(SCO);
r6=r3 +|- r7, r2=r3 -|+ r7(SCO);
                         
r0=r1 +|- r2, r7=r1 -|+ r2(ASR);
r3=r4 +|- r5, r6=r4 -|+ r5(ASR);
r6=r7 +|- r0, r5=r7 -|+ r0(ASR);
r1=r2 +|- r3, r4=r2 -|+ r3(ASR);
r4=r3 +|- r5, r3=r3 -|+ r5(ASR);
r6=r3 +|- r7, r2=r3 -|+ r7(ASR);
                         

r0=r1 +|- r2, r7=r1 -|+ r2(ASL);
r3=r4 +|- r5, r6=r4 -|+ r5(ASL);
r6=r7 +|- r0, r5=r7 -|+ r0(ASL);
r1=r2 +|- r3, r4=r2 -|+ r3(ASL);
r4=r3 +|- r5, r3=r3 -|+ r5(ASL);
r6=r3 +|- r7, r2=r3 -|+ r7(ASL);
                         

r0=r1 +|- r2, r7=r1 -|+ r2(S,ASR);
r3=r4 +|- r5, r6=r4 -|+ r5(S,ASR);
r6=r7 +|- r0, r5=r7 -|+ r0(S,ASR);
r1=r2 +|- r3, r4=r2 -|+ r3(S,ASR);
r4=r3 +|- r5, r3=r3 -|+ r5(S,ASR);
r6=r3 +|- r7, r2=r3 -|+ r7(S,ASR);
                         

r0=r1 +|- r2, r7=r1 -|+ r2(CO,ASR);
r3=r4 +|- r5, r6=r4 -|+ r5(CO,ASR);
r6=r7 +|- r0, r5=r7 -|+ r0(CO,ASR);
r1=r2 +|- r3, r4=r2 -|+ r3(CO,ASR);
r4=r3 +|- r5, r3=r3 -|+ r5(CO,ASR);
r6=r3 +|- r7, r2=r3 -|+ r7(CO,ASR);
                         

r0=r1 +|- r2, r7=r1 -|+ r2(SCO,ASR);
r3=r4 +|- r5, r6=r4 -|+ r5(SCO,ASR);
r6=r7 +|- r0, r5=r7 -|+ r0(SCO,ASR);
r1=r2 +|- r3, r4=r2 -|+ r3(SCO,ASR);
r4=r3 +|- r5, r3=r3 -|+ r5(SCO,ASR);
r6=r3 +|- r7, r2=r3 -|+ r7(SCO,ASR);

r0=r1 +|- r2, r7=r1 -|+ r2(S,ASL);
r3=r4 +|- r5, r6=r4 -|+ r5(S,ASL);
r6=r7 +|- r0, r5=r7 -|+ r0(S,ASL);
r1=r2 +|- r3, r4=r2 -|+ r3(S,ASL);
r4=r3 +|- r5, r3=r3 -|+ r5(S,ASL);
r6=r3 +|- r7, r2=r3 -|+ r7(S,ASL);
                         

r0=r1 +|- r2, r7=r1 -|+ r2(CO,ASL);
r3=r4 +|- r5, r6=r4 -|+ r5(CO,ASL);
r6=r7 +|- r0, r5=r7 -|+ r0(CO,ASL);
r1=r2 +|- r3, r4=r2 -|+ r3(CO,ASL);
r4=r3 +|- r5, r3=r3 -|+ r5(CO,ASL);
r6=r3 +|- r7, r2=r3 -|+ r7(CO,ASL);
                         

r0=r1 +|- r2, r7=r1 -|+ r2(SCO,ASL);
r3=r4 +|- r5, r6=r4 -|+ r5(SCO,ASL);
r6=r7 +|- r0, r5=r7 -|+ r0(SCO,ASL);
r1=r2 +|- r3, r4=r2 -|+ r3(SCO,ASL);
r4=r3 +|- r5, r3=r3 -|+ r5(SCO,ASL);
r6=r3 +|- r7, r2=r3 -|+ r7(SCO,ASL);



//Dual 32-Bit Operations
//Dreg = Dreg + Dreg, Dreg = Dreg - Dreg (opt_mode_1) ; /* add, subtract; the set of source registers must be the same for each operation (b) */
r2=r0+r1, r3=r0-r1 ; /* 32-bit operations */

r7=r0+r1, r0=r0-r1 ; /* 32-bit operations */
r6=r1+r2, r1=r1-r2 ; /* 32-bit operations */
r5=r2+r3, r2=r2-r3 ; /* 32-bit operations */
r4=r3+r4, r3=r3-r4 ; /* 32-bit operations */
r3=r4+r5, r4=r4-r5 ; /* 32-bit operations */
r2=r5+r6, r5=r5-r6 ; /* 32-bit operations */
r1=r6+r7, r6=r6-r7 ; /* 32-bit operations */
r0=r7+r0, r7=r7-r0 ; /* 32-bit operations */

r2=r0+r1, r3=r0-r1(s) ; /* dual 32-bit operations with saturation */
r7=r0+r1, r0=r0-r1 (s); /* 32-bit operations */
r6=r1+r2, r1=r1-r2 (s); /* 32-bit operations */
r5=r2+r3, r2=r2-r3 (s); /* 32-bit operations */
r4=r3+r4, r3=r3-r4(s) ; /* 32-bit operations */
r3=r4+r5, r4=r4-r5 (s); /* 32-bit operations */
r2=r5+r6, r5=r5-r6 (s); /* 32-bit operations */
r1=r6+r7, r6=r6-r7 (s); /* 32-bit operations */
r0=r7+r0, r7=r7-r0 (s); /* 32-bit operations */



//Dual 40-Bit Accumulator Operations
//Dreg = A1 + A0, Dreg = A1 - A0 (opt_mode_1) ; /* add, subtract Accumulators; subtract A0 from A1 (b) */
r0=a1+a0, r1=a1-a0 ;
r2=a1+a0, r3=a1-a0 ;
r4=a1+a0, r5=a1-a0 ;
r6=a1+a0, r7=a1-a0 ;
r1=a1+a0, r0=a1-a0 ;
r3=a1+a0, r2=a1-a0 ;
r5=a1+a0, r4=a1-a0 ;

r0=a1+a0, r1=a1-a0 (s);
r2=a1+a0, r3=a1-a0 (s);
r4=a1+a0, r5=a1-a0 (s);
r6=a1+a0, r7=a1-a0 (s);
r1=a1+a0, r0=a1-a0 (s);
r3=a1+a0, r2=a1-a0 (s);
r5=a1+a0, r4=a1-a0 (s);

//Dreg = A0 + A1, Dreg = A0 - A1 (opt_mode_1) ; /* add, subtract Accumulators; subtract A1 from A0 (b) */
r4=a0+a1, r6=a0-a1(s);

r0=a0+a1, r1=a0-a1 ;
r2=a0+a1, r3=a0-a1 ;
r4=a0+a1, r5=a0-a1 ;
r6=a0+a1, r7=a0-a1 ;
r1=a0+a1, r0=a0-a1 ;
r3=a0+a1, r2=a0-a1 ;
r5=a0+a1, r4=a0-a1 ;

r0=a0+a1, r1=a0-a1 (s);
r2=a0+a1, r3=a0-a1 (s);
r4=a0+a1, r5=a0-a1 (s);
r6=a0+a1, r7=a0-a1 (s);
r1=a0+a1, r0=a0-a1 (s);
r3=a0+a1, r2=a0-a1 (s);
r5=a0+a1, r4=a0-a1 (s);

//Constant Shift Magnitude
//Dreg = Dreg >>> uimm4 (V) ; /* arithmetic shift right, immediate (b) */
R0 = R0 >>> 5(V);

R0 = R1 >>> 5(V);
R2 = R3 >>> 5(V);
R4 = R5 >>> 5(V);
R6 = R7 >>> 5(V);
R1 = R0 >>> 5(V);
R3 = R2 >>> 5(V);
R5 = R4 >>> 5(V);
R7 = R6 >>> 5(V);


//Dreg = Dreg << uimm4 (V,S) ; /* arithmetic shift left, immediate with saturation (b) */

R0 = R1 << 5(V,S);
R2 = R3 << 5(V,S);
R4 = R5 << 5(V,S);
R6 = R7 << 5(V,S);
R1 = R0 << 5(V,S);
R3 = R2 << 5(V,S);
R5 = R4 << 5(V,S);
R7 = R6 << 5(V,S);

//Registered Shift Magnitude
//Dreg = ASHIFT Dreg BY Dreg_lo (V) ; /* arithmetic shift (b) */
r2=ashift r7 by r5.l (v) ;

R0 = ASHIFT R1 BY R2.L (V);
R3 = ASHIFT R4 BY R5.L (V);
R6 = ASHIFT R7 BY R0.L (V);
R1 = ASHIFT R2 BY R3.L (V);
R4 = ASHIFT R5 BY R6.L (V);
R7 = ASHIFT R0 BY R1.L (V);
R2 = ASHIFT R3 BY R4.L (V);
R5 = ASHIFT R6 BY R7.L (V);


//Dreg = ASHIFT Dreg BY Dreg_lo (V, S) ; /* arithmetic shift with saturation (b) */
R0 = ASHIFT R1 BY R2.L (V,S);
R3 = ASHIFT R4 BY R5.L (V,S);
R6 = ASHIFT R7 BY R0.L (V,S);
R1 = ASHIFT R2 BY R3.L (V,S);
R4 = ASHIFT R5 BY R6.L (V,S);
R7 = ASHIFT R0 BY R1.L (V,S);
R2 = ASHIFT R3 BY R4.L (V,S);
R5 = ASHIFT R6 BY R7.L (V,S);

//Constant Shift Magnitude
//Dreg = Dreg >> uimm4 (V) ; /* logical shift right, immediate (b) */
R0 = R1 >> 5(V);
R2 = R3 >> 5(V);
R4 = R5 >> 5(V);
R6 = R7 >> 5(V);
R1 = R0 >> 5(V);
R3 = R2 >> 5(V);
R5 = R4 >> 5(V);
R7 = R6 >> 5(V);

//Dreg = Dreg << uimm4 (V) ; /* logical shift left, immediate (b) */
R0 = R1 << 5(V);
R2 = R3 << 5(V);
R4 = R5 << 5(V);
R6 = R7 << 5(V);
R1 = R0 << 5(V);
R3 = R2 << 5(V);
R5 = R4 << 5(V);
R7 = R6 << 5(V);


//Registered Shift Magnitude
//Dreg = LSHIFT Dreg BY Dreg_lo (V) ; /* logical shift (b) */

R0 = LSHIFT R1 BY R2.L (V);
R3 = LSHIFT R4 BY R5.L (V);
R6 = LSHIFT R7 BY R0.L (V);
R1 = LSHIFT R2 BY R3.L (V);
R4 = LSHIFT R5 BY R6.L (V);
R7 = LSHIFT R0 BY R1.L (V);
R2 = LSHIFT R3 BY R4.L (V);
R5 = LSHIFT R6 BY R7.L (V);

//Dreg = MAX ( Dreg , Dreg ) (V) ; /* dual 16-bit operations (b) */
r7 = max (r1, r0) (v) ;

R0 = MAX (R1, R2) (V);
R3 = MAX (R4, R5) (V);
R6 = MAX (R7, R0) (V);
R1 = MAX (R2, R3) (V);
R4 = MAX (R5, R6) (V);
R7 = MAX (R0, R1) (V);
R2 = MAX (R3, R4) (V);
R5 = MAX (R6, R7) (V);

//Dreg = MIN ( Dreg , Dreg ) (V) ; /* dual 16-bit operation (b) */
R0 = MIN (R1, R2) (V);
R3 = MIN (R4, R5) (V);
R6 = MIN (R7, R0) (V);
R1 = MIN (R2, R3) (V);
R4 = MIN (R5, R6) (V);
R7 = MIN (R0, R1) (V);
R2 = MIN (R3, R4) (V);
R5 = MIN (R6, R7) (V);

r2.h=r7.l*r6.h, r2.l=r7.h*r6.h ;
/* simultaneous MAC0 and MAC1 execution, 16-bit results. Both
results are signed fractions. */
r4.l=r1.l*r0.l, r4.h=r1.h*r0.h ;
/* same as above. MAC order is arbitrary. */
r0.h=r3.h*r2.l (m), r0.l=r3.l*r2.l ;

a1=r2.l*r3.h, a0=r2.h*r3.h ;
/* both multiply signed fractions into separate Accumulators */
a0=r1.l*r0.l, a1+=r1.h*r0.h ;
/* same as above, but sum result into A1. MAC order is arbitrary.
*/
a1+=r3.h*r3.l, a0-=r3.h*r3.h ;
/* sum product into A1, subtract product from A0 */
a1=r3.h*r2.l (m), a0+=r3.l*r2.l ;
/* MAC1 multiplies a signed fraction in r3.h by an unsigned fraction
in r2.l. MAC0 multiplies two signed fractions. */
a1=r7.h*r4.h (m), a0+=r7.l*r4.l (fu) ;
/* MAC1 multiplies signed fraction by unsigned fraction. MAC0
multiplies and accumulates two unsigned fractions. */
a1+=r3.h*r2.h, a0=r3.l*r2.l (is) ;
/* both MACs perform signed integer multiplication */
a1=r6.h*r7.h, a0+=r6.l*r7.l (w32) ;
/* both MACs multiply signed fractions, sign extended, and saturate
both Accumulators at bit 31 */
r2.h=(a1=r7.l*r6.h), r2.l=(a0=r7.h*r6.h) ; /* simultaneous MAC0
and MAC1 execution, both are signed fractions, both products load
into the Accumulators,MAC1 into half-word registers. */
r4.l=(a0=r1.l*r0.l), r4.h=(a1+=r1.h*r0.h) ; /* same as above,
but sum result into A1. ; MAC order is arbitrary. */
r7.h=(a1+=r6.h*r5.l), r7.l=(a0=r6.h*r5.h) ; /* sum into A1,
subtract into A0 */
r0.h=(a1=r7.h*r4.l) (m), r0.l=(a0+=r7.l*r4.l) ; /* MAC1 multiplies
a signed fraction by an unsigned fraction. MAC0 multiplies
two signed fractions. */
r5.h=(a1=r3.h*r2.h) (m), r5.l=(a0+=r3.l*r2.l) (fu) ; /* MAC1
multiplies signed fraction by unsigned fraction. MAC0 multiplies
two unsigned fractions. */
r0.h=(a1+=r3.h*r2.h), r0.l=(a0=r3.l*r2.l) (is) ; /* both MACs
perform signed integer multiplication. */
r5.h=(a1=r2.h*r1.h), a0+=r2.l*r1.l ; /* both MACs multiply
signed fractions. MAC0 does not copy the accum result. */
r3.h=(a1=r2.h*r1.h) (m), a0=r2.l*r1.l ; /* MAC1 multiplies
signed fraction by unsigned fraction and uses all 40 bits of A1.
MAC0 multiplies two signed fractions. */
r3.h=a1, r3.l=(a0+=r0.l*r1.l) (s2rnd) ; /* MAC1 copies Accumulator
to register half. MAC0 multiplies signed fractions. Both
scale the result and round on the way to the destination register.
*/
r0.l=(a0+=r7.l*r6.l), r0.h=(a1+=r7.h*r6.h) (iss2) ; /* both
MACs process signed integer the way to the destination half-registers.
*/
r3=(a1=r6.h*r7.h), r2=(a0=r6.l*r7.l) ; /* simultaneous MAC0 and
MAC1 execution, both are signed fractions, both products load
into the Accumulators */
r4=(a0=r6.l*r7.l), r5=(a1+=r6.h*r7.h) ; /* same as above, but
sum result into A1. MAC order is arbitrary. */
r7=(a1+=r3.h*r5.h), r6=(a0-=r3.l*r5.l) ; /* sum into A1, subtract
into A0 */
r1=(a1=r7.l*r4.l) (m), r0=(a0+=r7.h*r4.h) ; /* MAC1 multiplies
a signed fraction by an unsigned fraction. MAC0 multiplies two
signed fractions. */
r5=(a1=r3.h*r7.h) (m), r4=(a0+=r3.l*r7.l) (fu) ; /* MAC1 multiplies
signed fraction by unsigned fraction. MAC0 multiplies two
unsigned fractions. */
r1=(a1+=r3.h*r2.h), r0=(a0=r3.l*r2.l) (is) ; /* both MACs perform
signed integer multiplication */
r5=(a1-=r6.h*r7.h), a0+=r6.l*r7.l ; /* both MACs multiply
signed fractions. MAC0 does not copy the accum result */
r3=(a1=r6.h*r7.h) (m), a0-=r6.l*r7.l ; /* MAC1 multiplies
signed fraction by unsigned fraction and uses all 40 bits of A1.
MAC0 multiplies two signed fractions. */
r3=a1, r2=(a0+=r0.l*r1.l) (s2rnd) ; /* MAC1 moves Accumulator
to register. MAC0 multiplies signed fractions. Both scale the
result and round on the way to the destination register. */
r0=(a0+=r7.l*r6.l), r1=(a1+=r7.h*r6.h) (iss2) ; /* both MACs
process signed integer operands and scale the result on the way
to the destination registers. */

r5 =-r3 (v) ; /* R5.H becomes the negative of R3.H and R5.L
becomes the negative of R3.L If r3 = 0x0004 7FFF the result is r5
= 0xFFFC 8001 */

r3=pack(r4.l, r5.l) ; /* pack low / low half-words */
r1=pack(r6.l, r4.h) ; /* pack low / high half-words */
r0=pack(r2.h, r4.l) ; /* pack high / low half-words */
r5=pack(r7.h, r2.h) ; /* pack high / high half-words */

(r1,r0) = SEARCH R2 (LE) || R2=[P0++];
/* search for the last minimum in all but the
last element of the array */
(r1,r0) = SEARCH R2 (LE);

saa (r1:0, r3:2) || r0=[i0++] || r2=[i1++] ;
saa (r1:0, r3:2)(r) || r1=[i0++] || r3=[i1++] ;
mnop || r1 = [i0++] || r3 = [i1++] ;
r7.h=r7.l=sign(r2.h)*r3.h + sign(r2.l)*r3.l || i0+=m3 || r0=[i0]
;

/* Add/subtract two vector values while incrementing an Ireg and
loading a data register. */
R2 = R2 +|+ R4, R4 = R2 -|- R4 (ASR) || I0 += M0 (BREV) || R1 = [I0] ;
/* Multiply and accumulate to Accumulator while loading a data
register and storing a data register using an Ireg pointer. */
A1=R2.L*R1.L, A0=R2.H*R1.H || R2.H=W[I2++] || [I3++]=R3 ;
/* Multiply and accumulate while loading two data registers. One
load uses an Ireg pointer. */
A1+=R0.L*R2.H,A0+=R0.L*R2.L || R2.L=W[I2++] || R0=[I1--] ;
R3.H=(A1+=R0.L*R1.H), R3.L=(A0+=R0.L*R1.L) || R0=[P0++] || R1=[I0] ;
/* Pack two vector values while storing a data register using an
Ireg pointer and loading another data register. */
R1=PACK(R1.H,R0.H) || [I0++]=R0 || R2.L=W[I2++] ;

/* Multiply-Accumulate to a Data register while incrementing an
Ireg. */
r6=(a0+=r3.h*r2.h)(fu) || i2-=m0 ;
/* which the assembler expands into:
r6=(a0+=r3.h*r2.h)(fu) || i2-=m0 || nop ; */

/* Test for ensure (m) is not thown away.  */
r0.l=r3.l*r2.l, r0.h=r3.h*r2.l (m) ;
R2 = R7.L * R0.L, R3 = R7.L * R0.H (m);
R2 = (A0 = R7.L * R0.L), R3 = ( A1 = R7.L * R0.H) (m);
