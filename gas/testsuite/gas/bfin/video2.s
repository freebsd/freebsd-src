
.EXTERN MY_LABEL2;
.section .text;

//
//13 VIDEO PIXEL OPERATIONS
//

//Dreg = ALIGN8 ( Dreg, Dreg ) ; /* overlay 1 byte (b) */
R0 = ALIGN8(R0, R0);
R0 = ALIGN8(R0, R1);
R0 = ALIGN8(R1, R0);
R0 = ALIGN8(R1, R1);
R0 = ALIGN8(R1, R2);
R3 = ALIGN8(R4, R5);
R6 = ALIGN8(R7, R0);
R1 = ALIGN8(R2, R3);
R4 = ALIGN8(R5, R6);
R7 = ALIGN8(R0, R1);
R2 = ALIGN8(R3, R4);
R5 = ALIGN8(R6, R7);

//Dreg = ALIGN16 ( Dreg, Dreg ) ; /* overlay 2 bytes (b) */
R0 = ALIGN16(R0, R0);
R0 = ALIGN16(R0, R1);
R0 = ALIGN16(R1, R0);
R0 = ALIGN16(R1, R1);
R0 = ALIGN16(R1, R2);
R3 = ALIGN16(R4, R5);
R6 = ALIGN16(R7, R0);
R1 = ALIGN16(R2, R3);
R4 = ALIGN16(R5, R6);
R7 = ALIGN16(R0, R1);
R2 = ALIGN16(R3, R4);
R5 = ALIGN16(R6, R7);

//Dreg = ALIGN24 ( Dreg, Dreg ) ; /* overlay 3 bytes (b) */
R0 = ALIGN24(R0, R0);
R0 = ALIGN24(R0, R1);
R0 = ALIGN24(R1, R0);
R0 = ALIGN24(R1, R1);
R0 = ALIGN24(R1, R2);
R3 = ALIGN24(R4, R5);
R6 = ALIGN24(R7, R0);
R1 = ALIGN24(R2, R3);
R4 = ALIGN24(R5, R6);
R7 = ALIGN24(R0, R1);
R2 = ALIGN24(R3, R4);
R5 = ALIGN24(R6, R7);

DISALGNEXCPT ; /* (b) */

/* forward byte order operands */
//Dreg = BYTEOP3P (Dreg_pair, Dreg_pair) (LO) ; /* sum into low bytes (b) */
//Dreg = BYTEOP3P (Dreg_pair, Dreg_pair) (HI) ; /* sum into high bytes (b) */
/* reverse byte order operands */
//Dreg = BYTEOP3P (Dreg_pair, Dreg_pair) (LO, R) ; /* sum into low bytes (b) */
//Dreg = BYTEOP3P (Dreg_pair, Dreg_pair) (HI, R) ; /* sum into high bytes (b) */

r0 = byteop3p (r1:0, r3:2) (lo) ;
r1 = byteop3p (r1:0, r3:2) (hi) ;
r2 = byteop3p (r1:0, r3:2) (lo, r) ;
r3 = byteop3p (r1:0, r3:2) (hi, r) ;
r4 = byteop3p (r3:2, r1:0) (lo) ;
r5 = byteop3p (r3:2, r1:0) (hi) ;
r6 = byteop3p (r3:2, r1:0) (lo, r) ;
r7 = byteop3p (r3:2, r1:0) (hi, r) ;

//Dreg = A1.L + A1.H, Dreg = A0.L + A0.H ; /* (b) */

R0 = A1.L + A1.H, R0= A0.L + A0.H ;
R0 = A1.L + A1.H, R1= A0.L + A0.H ;
R2 = A1.L + A1.H, R3= A0.L + A0.H ;
R4 = A1.L + A1.H, R5= A0.L + A0.H ;
R6 = A1.L + A1.H, R7= A0.L + A0.H ;

/* forward byte order operands */
//( Dreg, Dreg ) = BYTEOP16P ( Dreg_pair, Dreg_pair ) ; /* (b) */
(r7,r0) = BYTEOP16P ( r3:2,r1:0 ) ;
(r1,r2) = byteop16p (r3:2,r1:0) ;
(r0,r1) = BYTEOP16P ( r3:2,r1:0 ) ;
(r2,r3) = byteop16p (r3:2,r1:0) ;
(r7,r0) = BYTEOP16P (r1:0, r3:2) ;
(r1,r2) = byteop16p (r1:0,r3:2) ;
(r0,r1) = BYTEOP16P (r1:0, r3:2) ;
(r2,r3) = byteop16p (r1:0,r3:2) ;

/* reverse byte order operands */
//( Dreg, Dreg ) = BYTEOP16P ( Dreg_pair, Dreg_pair ) (R); /* (b) */
(r7,r0) = BYTEOP16P ( r3:2,r1:0 )(r) ;
(r1,r2) = byteop16p (r3:2,r1:0)(r) ;
(r0,r1) = BYTEOP16P ( r3:2,r1:0 )(r) ;
(r2,r3) = byteop16p (r3:2,r1:0)(r) ;
(r7,r0) = BYTEOP16P (r1:0, r3:2)(r) ;
(r1,r2) = byteop16p (r1:0,r3:2)(r) ;
(r0,r1) = BYTEOP16P (r1:0, r3:2)(r) ;
(r2,r3) = byteop16p (r1:0,r3:2)(r) ;

/* forward byte order operands */
//Dreg = BYTEOP1P (Dreg_pair, Dreg_pair) ; /* (b) */
//Dreg = BYTEOP1P (Dreg_pair, Dreg_pair) (T) ; /* truncated (b)*/
/* reverse byte order operands */
//Dreg = BYTEOP1P (Dreg_pair, Dreg_pair) (R) ; /* (b) */
//Dreg = BYTEOP1P (Dreg_pair, Dreg_pair) (T, R) ; /* truncated (b) */				                

r3 = byteop1p (r1:0, r3:2) ;
r3 = byteop1p (r1:0, r3:2) (r) ;
r3 = byteop1p (r1:0, r3:2) (t) ;
r3 = byteop1p (r1:0, r3:2) (t,r) ;

r0 = byteop1p (r3:2,r1:0);
r1 = byteop1p (r3:2,r1:0)(r) ;
r2 = byteop1p (r3:2,r1:0)(t) ;
r3 = byteop1p (r3:2,r1:0)(t,r) ;

/* forward byte order operands */
//Dreg = BYTEOP2P (Dreg_pair, Dreg_pair) (RNDL) ;
/* round into low bytes (b) */
//Dreg = BYTEOP2P (Dreg_pair, Dreg_pair) (RNDH) ;
/* round into high bytes (b) */
//Dreg = BYTEOP2P (Dreg_pair, Dreg_pair) (TL) ;
/* truncate into low bytes (b) */
//Dreg = BYTEOP2P (Dreg_pair, Dreg_pair) (TH) ;
/* truncate into high bytes (b) */
/* reverse byte order operands */
//Dreg = BYTEOP2P (Dreg_pair, Dreg_pair) (RNDL, R) ;
/* round into low bytes (b) */
//Dreg = BYTEOP2P (Dreg_pair, Dreg_pair) (RNDH, R) ;
/* round into high bytes (b) */
//Dreg = BYTEOP2P (Dreg_pair, Dreg_pair) (TL, R) ;
/* truncate into low bytes (b) */
//Dreg = BYTEOP2P (Dreg_pair, Dreg_pair) (TH, R) ;
/* truncate into high bytes (b) */

r3 = byteop2p (r1:0, r3:2) (rndl) ;
r3 = byteop2p (r1:0, r3:2) (rndh) ;
r3 = byteop2p (r1:0, r3:2) (tl) ;
r3 = byteop2p (r1:0, r3:2) (th) ;
r3 = byteop2p (r1:0, r3:2) (rndl, r) ;
r3 = byteop2p (r1:0, r3:2) (rndh, r) ;
r3 = byteop2p (r1:0, r3:2) (tl, r) ;
r3 = byteop2p (r1:0, r3:2) (th, r) ;

r0 = byteop2p (r1:0, r3:2) (rndl) ;
r1 = byteop2p (r1:0, r3:2) (rndh) ;
r2 = byteop2p (r1:0, r3:2) (tl) ;
r3 = byteop2p (r1:0, r3:2) (th) ;
r4 = byteop2p (r1:0, r3:2) (rndl, r) ;
r5 = byteop2p (r1:0, r3:2) (rndh, r) ;
r6 = byteop2p (r1:0, r3:2) (tl, r) ;
r7 = byteop2p (r1:0, r3:2) (th, r) ;

r0 = byteop2p (r3:2, r3:2) (rndl) ;
r1 = byteop2p (r3:2, r3:2) (rndh) ;
r2 = byteop2p (r3:2, r3:2) (tl) ;
r3 = byteop2p (r3:2, r3:2) (th) ;
r4 = byteop2p (r3:2, r3:2) (rndl, r) ;
r5 = byteop2p (r3:2, r3:2) (rndh, r) ;
r6 = byteop2p (r3:2, r3:2) (tl, r) ;
r7 = byteop2p (r3:2, r3:2) (th, r) ;

//Dreg = BYTEPACK ( Dreg, Dreg ) ; /* (b) */
r0 = bytepack (r0,r0) ;
r1 = bytepack (r2,r3) ;
r4 = bytepack (r5,r6) ;
r7 = bytepack (r0,r1) ;
r2 = bytepack (r3,r4) ;
r5 = bytepack (r6,r7) ;

/* forward byte order operands */
//(Dreg, Dreg) = BYTEOP16M (Dreg_pair, Dreg_pair) ; /* (b */)
/* reverse byte order operands */
//(Dreg, Dreg) = BYTEOP16M (Dreg-pair, Dreg-pair) (R) ; /* (b) */

(r1,r2)= byteop16m (r3:2,r1:0) ;
(r1,r2)= byteop16m (r3:2,r1:0) (r) ;
(r0,r1)= byteop16m (r3:2,r1:0) ;
(r2,r3)= byteop16m (r3:2,r1:0) (r) ;
(r3,r5)= byteop16m (r3:2,r1:0) ;
(r6,r7)= byteop16m (r3:2,r1:0) (r) ;

(r1,r2)= byteop16m (r1:0,r1:0) ;
(r1,r2)= byteop16m (r1:0,r1:0) (r) ;
(r0,r1)= byteop16m (r1:0,r1:0) ;
(r2,r3)= byteop16m (r1:0,r1:0) (r) ;
(r3,r5)= byteop16m (r1:0,r1:0) ;
(r6,r7)= byteop16m (r1:0,r1:0) (r) ;

(r1,r2)= byteop16m (r1:0,r3:2) ;
(r1,r2)= byteop16m (r1:0,r3:2) (r) ;
(r0,r1)= byteop16m (r1:0,r3:2) ;
(r2,r3)= byteop16m (r1:0,r3:2) (r) ;
(r3,r5)= byteop16m (r1:0,r3:2) ;
(r6,r7)= byteop16m (r1:0,r3:2) (r) ;

(r1,r2)= byteop16m (r3:2,r3:2) ;
(r1,r2)= byteop16m (r3:2,r3:2) (r) ;
(r0,r1)= byteop16m (r3:2,r3:2) ;
(r2,r3)= byteop16m (r3:2,r3:2) (r) ;
(r3,r5)= byteop16m (r3:2,r3:2) ;
(r6,r7)= byteop16m (r3:2,r3:2) (r) ;

//SAA (Dreg_pair, Dreg_pair) ; /* forward byte order operands (b) */
//SAA (Dreg_pair, Dreg_pair) (R) ; /* reverse byte order operands (b) */

saa(r1:0, r3:2) || r0 = [i0++] || r2 = [i1++] ; /* parallel fill instructions */
saa (r1:0, r3:2) (R) || r1 = [i0++] || r3 = [i1++] ; /* reverse, parallel fill instructions */
saa (r1:0, r3:2) ; /* last SAA in a loop, no more fill required */

//( Dreg , Dreg ) = BYTEUNPACK Dreg_pair ; /* (b) */
//( Dreg , Dreg ) = BYTEUNPACK Dreg_pair (R) ; /* reverse source order (b) */

(r6,r5) = byteunpack r1:0 ; /* non-reversing sources */
(r6,r5) = byteunpack r1:0 (R) ; /* reversing sources case */
(r6,r5) = byteunpack r3:2 ; /* non-reversing sources */
(r6,r5) = byteunpack r3:2 (R) ; /* reversing sources case */
(r0,r1) = byteunpack r1:0 ; /* non-reversing sources */
(r2,r3) = byteunpack r1:0 (R) ; /* reversing sources case */
(r4,r5) = byteunpack r3:2 ; /* non-reversing sources */
(r6,r7) = byteunpack r3:2 (R) ; /* reversing sources case */
