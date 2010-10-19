
.EXTERN MY_LABEL2;
.section .text;

//
//7 LOGICAL OPERATIONS
//

//Dreg = Dreg & Dreg ; /* (a) */

R7 = R7 & R7;
R7 = R7 & R0;
r7 = R7 & R1;

R1 = R7 & R7;
R2 = R7 & R0;
r3 = R7 & R1;

//Dreg = ~ Dreg ; /* (a)*/

R7 = ~R7;
R7 = ~R0;
R0 = ~R7;
R0 = ~R2;

//Dreg = Dreg | Dreg ; /* (a) */

R7 = R7 | R7;
R7 = R7 | R1;
R7 = R7 | R0;

R1 = R7 | R7;
R2 = R7 | R1;
R3 = R7 | R0;

//Dreg = Dreg ^ Dreg ; /* (a) */

R7 = R7 ^ R7;
R7 = R7 ^ R1;
R7 = R7 ^ R0;

R1 = R7 ^ R7;
R2 = R7 ^ R1;
R3 = R7 ^ R0;

//Dreg_lo = CC = BXORSHIFT ( A0, Dreg ) ; /* (b) */
R0.L = CC = BXORSHIFT(A0, R0);
R0.L = CC = BXORSHIFT(A0, R1);

R3.L = CC = BXORSHIFT(A0, R0);
R3.L = CC = BXORSHIFT(A0, R1);

//Dreg_lo = CC = BXOR ( A0, Dreg ) ; /* (b) */
R0.L = CC = BXOR(A0, R0);
R0.L = CC = BXOR(A0, R1);

R3.L = CC = BXOR(A0, R0);
R3.L = CC = BXOR(A0, R1);

//Dreg_lo = CC = BXOR ( A0, A1, CC ) ; /* (b) */
R0.L = CC = BXOR(A0, A1, CC);
R0.L = CC = BXOR(A0, A1, CC);

R3.L = CC = BXOR(A0, A1, CC);
R3.L = CC = BXOR(A0, A1, CC);

A0 = BXORSHIFT ( A0, A1, CC ) ; /* (b) */


