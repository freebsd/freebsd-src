
.EXTERN MY_LABEL2;
.section .text;

//
//9 SHIFT/ROTATE OPERATIONS
//

//Preg = ( Preg + Preg ) << 1 ; /* dest_reg = (dest_reg + src_reg) x 2 (a) */
P0 = (P0+P0)<<1;
P0 = (P0+P1)<<1;
P2 = (P2+P0)<<1;
P1 = (P1+P2)<<1;

//P0 = (P2+P0)<<1;

//Preg = ( Preg + Preg ) << 2 ; /* dest_reg = (dest_reg + src_reg) x 4 (a) */
P0 = (P0+P0)<<2;
P0 = (P0+P1)<<2;
P2 = (P2+P0)<<2;
P1 = (P1+P2)<<2;

//P0 = (P2+P0)<<2;

//Dreg = (Dreg + Dreg) << 1 ; /* dest_reg = (dest_reg + src_reg) x 2 (a) */
R0 = (R0+R0)<<1;
R0 = (R0+R1)<<1;
R2 = (R2+R0)<<1;
R1 = (R1+R2)<<1;

//R0 = (R2+R0)<<1;


//Dreg = (Dreg + Dreg) << 2 ; /* dest_reg = (dest_reg + src_reg) x 4 (a) */
R0 = (R0+R0)<<2;
R0 = (R0+R1)<<2;
R2 = (R2+R0)<<2;
R1 = (R1+R2)<<2;

//R0 = (R2+R0)<<2;

//Preg = Preg + ( Preg << 1 ) ; /* adder_pntr + (src_pntr x 2) (a) */
P0 = P0 + (P0 << 1);
P0 = P0 + (P1 << 1);
P0 = P0 + (P2 << 1);
P0 = P1 + (P2 << 1);
P0 = P2 + (P3 << 1);
P1 = P0 + (P0 << 1);
P1 = P0 + (P1 << 1);
P1 = P0 + (P2 << 1);
P1 = P1 + (P2 << 1);
P1 = P2 + (P3 << 1);

//Preg = Preg + ( Preg << 2 ) ; /* adder_pntr + (src_pntr x 4) (a) */
P0 = P0 + (P0 << 2);
P0 = P0 + (P1 << 2);
P0 = P0 + (P2 << 2);
P0 = P1 + (P2 << 2);
P0 = P2 + (P3 << 2);
P1 = P0 + (P0 << 2);
P1 = P0 + (P1 << 2);
P1 = P0 + (P2 << 2);
P1 = P1 + (P2 << 2);
P1 = P2 + (P3 << 2);

//Dreg >>>= uimm5 ; /* arithmetic right shift (a) */
R0 >>>= 0;
R0 >>>= 31;
R0 >>>= 5;
R5 >>>= 0;
R5 >>>= 31;
R5 >>>= 5;

//Dreg <<= uimm5 ; /* logical left shift (a) */
R0 <<= 0;
R0 <<= 31;
R0 <<= 5;
R5 <<= 0;
R5 <<= 31;
R5 <<= 5;
//Dreg_lo_hi = Dreg_lo_hi >>> uimm4 ; /* arithmetic right shift (b) */
R0.L = R0.L >>> 0;
R0.L = R0.L >>> 15;
R0.L = R0.H >>> 0;
R0.L = R0.H >>> 15;
R0.H = R0.L >>> 0;
R0.H = R0.L >>> 15;
R0.H = R0.H >>> 0;
R0.H = R0.H >>> 15;

R0.L = R1.L >>> 0;
R0.L = R1.L >>> 15;
R0.L = R1.H >>> 0;
R0.L = R1.H >>> 15;
R0.H = R1.L >>> 0;
R0.H = R1.L >>> 15;
R0.H = R1.H >>> 0;
R0.H = R1.H >>> 15;

R0.L = R7.L >>> 0;
R1.L = R6.L >>> 15;
R2.L = R5.H >>> 0;
R3.L = R4.H >>> 15;
R4.H = R3.L >>> 0;
R5.H = R2.L >>> 15;
R6.H = R1.H >>> 0;
R7.H = R0.H >>> 15;

//Dreg_lo_hi = Dreg_lo_hi << uimm4 (S) ; /* arithmetic left shift (b) */
R0.L = R0.L << 0(S);
R0.L = R0.L << 15(S);
R0.L = R0.H << 0(S);
R0.L = R0.H << 15(S);
R0.H = R0.L << 0(S);
R0.H = R0.L << 15(S);
R0.H = R0.H << 0(S);
R0.H = R0.H << 15(S);

R0.L = R1.L << 0(S);
R0.L = R1.L << 15(S);
R0.L = R1.H << 0(S);
R0.L = R1.H << 15(S);
R0.H = R1.L << 0(S);
R0.H = R1.L << 15(S);
R0.H = R1.H << 0(S);
R0.H = R1.H << 15(S);

R0.L = R7.L << 0(S);
R1.L = R6.L << 15(S);
R2.L = R5.H << 0(S);
R3.L = R4.H << 15(S);
R4.H = R3.L << 0(S);
R5.H = R2.L << 15(S);
R6.H = R1.H << 0(S);
R7.H = R0.H << 15(S);
//Dreg = Dreg >>> uimm5 ; /* arithmetic right shift (b) */
R0 = R0 >>> 0;
R0 = R0 >>> 31;
R0 = R1 >>> 0;
R0 = R1 >>> 31;
R7 = R0 >>> 0;
R6 = R1 >>> 31;
R5 = R2 >>> 0;
R4 = R3 >>> 31;
R3 = R4 >>> 0;
R2 = R5 >>> 31;
R1 = R6 >>> 0;
R0 = R7 >>> 31;

//Dreg = Dreg << uimm5 (S) ; /* arithmetic left shift (b) */
R0 = R0 << 0(S);
R0 = R0 << 31(S);
R0 = R1 << 0(S);
R0 = R1 << 31(S);
R7 = R0 << 0(S);
R6 = R1 << 31(S);
R5 = R2 << 0(S);
R4 = R3 << 31(S);
R3 = R4 << 0(S);
R2 = R5 << 31(S);
R1 = R6 << 0(S);
R0 = R7 << 31(S);
//A0 = A0 >>> uimm5 ; /* arithmetic right shift (b) */
A0 = A0 >>> 0;
A0 = A0 >>> 15;
A0 = A0 >>> 31;

//A0 = A0 << uimm5 ; /* logical left shift (b) */
A0 = A0 << 0;
A0 = A0 << 15;
A0 = A0 << 31;

//A1 = A1 >>> uimm5 ; /* arithmetic right shift (b) */
A1 = A1 >>> 0;
A1 = A1 >>> 15;
A1 = A1 >>> 31;

//A1 = A1 << uimm5 ; /* logical left shift (b) */
A1 = A1 << 0;
A1 = A1 << 15;
A1 = A1 << 31;

//Dreg >>>= Dreg ; /* arithmetic right shift (a) */
R0 >>>= R0;
R0 >>>= R1;
R1 >>>= R0;
R1 >>>= R7;

//Dreg <<= Dreg ; /* logical left shift (a) */
R0 <<= R0;
R0 <<= R1;
R1 <<= R0;
R1 <<= R7;

//Dreg_lo_hi = ASHIFT Dreg_lo_hi BY Dreg_lo (opt_sat) ; /* arithmetic right shift (b) */
r3.l = ashift r0.h by r7.l ; /* shift, half-word */
r3.h = ashift r0.l by r7.l ;
r3.h = ashift r0.h by r7.l ;
r3.l = ashift r0.l by r7.l ;
r3.l = ashift r0.h by r7.l(s) ; /* shift, half-word, saturated */
r3.h = ashift r0.l by r7.l(s) ; /* shift, half-word, saturated */
r3.h = ashift r0.h by r7.l(s) ;
r3.l = ashift r0.l by r7.l (s) ;

//Dreg = ASHIFT Dreg BY Dreg_lo (opt_sat) ; /* arithmetic right shift (b) */
r4 = ashift r2 by r7.l ; /* shift, word */
r4 = ashift r2 by r7.l (s) ; /* shift, word, saturated */

//A0 = ASHIFT A0 BY Dreg_lo ; /* arithmetic right shift (b)*/
A0 = ashift A0 by r7.l ; /* shift, Accumulator */

//A1 = ASHIFT A1 BY Dreg_lo ; /* arithmetic right shift (b)*/
A1 = ashift A1 by r7.l ; /* shift, Accumulator */

p3 = p2 >> 1 ; /* pointer right shift by 1 */
p3 = p3 >> 2 ; /* pointer right shift by 2 */
p4 = p5 << 1 ; /* pointer left shift by 1 */
p0 = p1 << 2 ; /* pointer left shift by 2 */
r3 >>= 17 ; /* data right shift */
r3 <<= 17 ; /* data left shift */
r3.l = r0.l >> 4 ; /* data right shift, half-word register */
r3.l = r0.h >> 4 ; /* same as above; half-word register combinations are arbitrary */
r3.h = r0.l << 12 ; /* data left shift, half-word register */
r3.h = r0.h << 14 ; /* same as above; half-word register combinations are arbitrary */

r3 = r6 >> 4 ; /* right shift, 32-bit word */
r3 = r6 << 4 ; /* left shift, 32-bit word */

a0 = a0 >> 7 ; /* Accumulator right shift */
a1 = a1 >> 25 ; /* Accumulator right shift */
a0 = a0 << 7 ; /* Accumulator left shift */
a1 = a1 << 14 ; /* Accumulator left shift */

r3 >>= r0 ; /* data right shift */
r3 <<= r1 ; /* data left shift */

r3.l = lshift r0.l by r2.l ; /* shift direction controlled by sign of R2.L */
r3.h = lshift r0.l by r2.l ;

a0 = lshift a0 by r7.l ;
a1 = lshift a1 by r7.l ;

r4 = rot r1 by 31 ; /* rotate left */
r4 = rot r1 by -32 ; /* rotate right */
r4 = rot r1 by 5 ; /* rotate right */

a0 = rot a0 by 22 ; /* rotate Accumulator left */
a0 = rot a0 by -32 ; /* rotate Accumulator left */
a0 = rot a0 by 31 ; /* rotate Accumulator left */

a1 = rot a1 by -32 ; /* rotate Accumulator right */
a1 = rot a1 by 31 ; /* rotate Accumulator right */
a1 = rot a1 by 22 ; /* rotate Accumulator right */

r4 = rot r1 by r2.l ;
a0 = rot a0 by r3.l ;
a1 = rot a1 by r7.l ;

r0.l = r1.l << 0;
r0.l = r1.l << 1;
r0.l = r1.l << 2;
r0.l = r1.l << 4;
r0.l = r1.l >> 0;
r0.l = r1.l >> 1;
r0.l = r1.l >> 2;
r0.l = r1.l >> 4;
r0.l = r1.l >>> 1;
r0.l = r1.l >>> 2;
r0.l = r1.l >>> 4;

r0.l = r1.h << 0;
r0.l = r1.h << 1;
r0.l = r1.h << 2;
r0.l = r1.h << 4;
r0.l = r1.h >> 0;
r0.l = r1.h >> 1;
r0.l = r1.h >> 2;
r0.l = r1.h >> 4;
r0.l = r1.h >>> 1;
r0.l = r1.h >>> 2;
r0.l = r1.h >>> 4;

r0.l = r1.h << 0 (S);
r0.l = r1.h << 1 (S);
r0.l = r1.h << 2 (S);
r0.l = r1.h << 4 (S);
r0.l = r1.h >>> 1 (S);
r0.l = r1.h >>> 2 (S);
r0.l = r1.h >>> 4 (S);

