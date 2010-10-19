
.EXTERN MY_LABEL2;
.section .text;

//
//4 MOVE
//

//genreg = genreg ; /* (a) */
R0 = R0;
R1 = R1;
R2 = R2;
R3 = R3;
R4 = R4;
R5 = R5;
R6 = R6;
R7 = R7;
	   
P0 = P0;
P1 = P1;
P2 = P2;
P3 = P3;
P4 = P4;
P5 = P5;
SP = SP;
FP = FP;

A0.X = A0.X;
A0.W = A0.W;
A1.X = A1.X;
A1.W = A1.W;


R0 = A1.W;
R1 = A1.X;
R2 = A0.W;
R3 = A0.X;
R4 = FP;
R5 = SP;
R6 = P5;
R7 = P4;
	   
P0 = P3;
P1 = P2;
P2 = P1;
P3 = P0;
P4 = R7;
P5 = R6;
SP = R5;
FP = R4;

A0.X = R3;
A0.W = R2;
A1.X = R1;
A1.W = R0;

A0.X = A0.W;
A0.X = A1.W;
A0.X = A1.X;

A1.X = A1.W;
A1.X = A0.W;
A1.X = A0.X;

A0.W = A0.W;
A0.W = A1.W;
A0.W = A1.X;

A1.W = A1.W;
A1.W = A0.W;
A1.W = A0.X;

//genreg = dagreg ; /* (a) */
R0 = I0;
R1 = I1;
R2 = I2;
R3 = I3;
R4 = M0;
R5 = M1;
R6 = M2;
R7 = M3;
	   
R0 = B0;
R1 = B1;
R2 = B2;
R3 = B3;
R4 = L0;
R5 = L1;
R6 = L2;
R7 = L3;

P0 = I0;
P1 = I1;
P2 = I2;
P3 = I3;
P4 = M0;
P5 = M1;
SP = M2;
FP = M3;
	   
P0 = B0;
P1 = B1;
P2 = B2;
P3 = B3;
P4 = L0;
P5 = L1;
SP = L2;
FP = L3;


A0.X = I0;
A0.W = I1;
A1.X = I2;
A1.W = I3;

A0.X = M0;
A0.W = M1;
A1.X = M2;
A1.W = M3;

A0.X = B0;
A0.W = B1;
A1.X = B2;
A1.W = B3;

A0.X = L0;
A0.W = L1;
A1.X = L2;
A1.W = L3;

//dagreg = genreg ; /* (a) */
I0 = R0;
I1 = P0;
I2 = SP;
I3 = FP;
I0 = A0.X;
I1 = A0.W;
I2 = A1.X;
I3 = A1.W;

M0 = R0;
M1 = P0;
M2 = SP;
M3 = FP;
M0 = A0.X;
M1 = A0.W;
M2 = A1.X;
M3 = A1.W;

B0 = R0;
B1 = P0;
B2 = SP;
B3 = FP;
B0 = A0.X;
B1 = A0.W;
B2 = A1.X;
B3 = A1.W;

L0 = R0;
L1 = P0;
L2 = SP;
L3 = FP;
L0 = A0.X;
L1 = A0.W;
L2 = A1.X;
L3 = A1.W;


//dagreg = dagreg ; /* (a) */

I0 = I1;
I1 = M0;
I2 = B1;
I3 = L0;

M0 = I1;
M1 = M0;
M2 = B1;
M3 = L0;

B0 = I1;
B1 = M0;
B2 = B1;
B3 = L0;

L0 = I1;
L1 = M0;
L2 = B1;
L3 = L0;

//genreg = USP ; /* (a)*/
R1 = USP;
P2 = USP;
SP = USP;
FP = USP;
A0.X = USP;
A1.W = USP;

//USP = genreg ; /* (a)*/
USP = R2;
USP = P4;
USP = SP;
USP = FP;
USP = A0.X;
USP = A1.W;

//Dreg = sysreg ; /* sysreg to 32-bit D-register (a) */
R0 = ASTAT;
R1 = SEQSTAT;
R2 = SYSCFG;
R3 = RETI;
R4 = RETX;
R5 = RETN;
R6 = RETE;
R7 = RETS;
R0 = LC0;
R1 = LC1;
R2 = LT0;
R3 = LT1;
R4 = LB0;
R5 = LB1;
R6 = CYCLES;
R7 = CYCLES2;
//R0 = EMUDAT; 
//sysreg = Dreg ; /* 32-bit D-register to sysreg (a) */
ASTAT = R0;
SEQSTAT = R1;
SYSCFG = R3;
RETI = R4;
RETX =R5;
RETN = R6;
RETE = R7;
RETS = R0;
LC0 = R1;
LC1 = R2;
LT0 = R3;
LT1 = R4;
LB0 = R5;
LB1 = R6;
CYCLES = R7;
CYCLES2 = R0;
//EMUDAT = R1; 
//sysreg = Preg ; /* 32-bit P-register to sysreg (a) */
ASTAT = P0;
SEQSTAT = P1;
SYSCFG = P3;
RETI = P4;
RETX =P5;
RETN = SP;
RETE = FP;
RETS = P0;
LC0 = P1;
LC1 = P2;
LT0 = P3;
LT1 = P4;
LB0 = P5;
LB1 = SP;
CYCLES = SP;
CYCLES2 = P0;
//EMUDAT = P1; 


//sysreg = USP ; /* (a) */
//ASTAT = USP;
//SEQSTAT = USP;
//SYSCFG = USP;
//RETI = USP;
//RETX =USP;
//RETN = USP;
//RETE = USP;
//RETS = USP;
//LC0 = USP;
//LC1 = USP;
//LT0 = USP;
//LT1 = USP;
//LB0 = USP;
//LB1 = USP;
//CYCLES = USP;
//CYCLES2 = USP;
//EMUDAT = USP; 

A0 = A1 ; /* move 40-bit Accumulator value (b) */

A1 = A0 ; /* move 40-bit Accumulator value (b) */

//A0 = Dreg ; /* 32-bit D-register to 40-bit A0, sign extended (b)*/
A0 = R0;
A0 = R1;
A0 = R2;

//A1 = Dreg ; /* 32-bit D-register to 40-bit A1, sign extended (b)*/

A1 = R0;
A1 = R1;
A1 = R2;
//Dreg_even = A0 (opt_mode) ; /* move 32-bit A0.W to even Dreg (b) */
R0 = A0;
R2 = A0(FU);
R4 = A0(ISS2);

//Dreg_odd = A1 (opt_mode) ; /* move 32-bit A1.W to odd Dreg (b) */
R1 = A1;
R3 = A1(FU);
R5 = A1(ISS2);

//Dreg_even = A0, Dreg_odd = A1 (opt_mode) ; /* move both Accumulators to a register pair (b) */
R0 = A0, R1 = A1;
R0 = A0, R1 = A1(FU);
R6 = A0, R7 = A1(ISS2);


//Dreg_odd = A1, Dreg_even = A0 (opt_mode) ; /* move both Accumulators to a register pair (b) */
R1 = A1, R0 = A0;
R3 = A1, R2 = A0(FU);
R5 = A1, R4 = A0(ISS2);

//IF CC DPreg = DPreg ; /* move if CC = 1 (a) */

IF CC R3 = R0;
IF CC R2 = R0;
IF CC R7 = R0;

IF CC R2 = P2;
IF CC R4 = P1;
IF CC R0 = P0;
IF CC R7 = P4;

IF CC P0 = P2;
IF CC P4 = P5;
IF CC P1 = P3;
IF CC P5 = P4;

IF CC P0 = R2;
IF CC P4 = R3;
IF CC P5 = R7;
IF CC P2 = R6;

//IF ! CC DPreg = DPreg ; /* move if CC = 0 (a) */
IF !CC R3 = R0;
IF !CC R2 = R0;
IF !CC R7 = R0;

IF !CC R2 = P2;
IF !CC R4 = P1;
IF !CC R0 = P0;
IF !CC R7 = P4;

IF !CC P0 = P2;
IF !CC P4 = P5;
IF !CC P1 = P3;
IF !CC P5 = P4;

IF !CC P0 = R2;
IF !CC P4 = R3;
IF !CC P5 = R7;
IF !CC P2 = R6;

//Dreg = Dreg_lo (Z) ; /* (a) */

R0 = R0.L(Z);
R2 = R1.L(Z);
R1 = R2.L(Z);
R7 = R6.L(Z);

//Dreg = Dreg_lo (X) ; /* (a)*/
R0 = R0.L(X);
R2 = R1.L(X);
R1 = R2.L(X);
R7 = R6.L(X);

R0 = R0.L;
R2 = R1.L;
R1 = R2.L;
R7 = R6.L;

//A0.X = Dreg_lo ; /* least significant 8 bits of Dreg into A0.X (b) */
A0.X = R0.L;
A0.X = R1.L;

//A1.X = Dreg_lo ; /* least significant 8 bits of Dreg into A1.X (b) */
A1.X = R0.L;
A1.X = R1.L;

//Dreg_lo = A0.X ; /* 8-bit A0.X, sign-extended, into least significant 16 bits of Dreg (b) */
R0.L = A0.X;
R1.L = A0.X;
R7.L = A0.X;

//Dreg_lo = A1.X ; /* 8-bit A1.X, sign-extended, into least significant 16 bits of Dreg (b) */
R0.L = A1.X;
R1.L = A1.X;
R7.L = A1.X;

//A0.L = Dreg_lo ; /* least significant 16 bits of Dreg into least significant 16 bits of A0.W (b) */
A0.L = R0.L;
A0.L = R1.L;
A0.L = R6.L;

//A1.L = Dreg_lo ; /* least significant 16 bits of Dreg into least significant 16 bits of A1.W (b) */
A1.L = R0.L;
A1.L = R1.L;
A1.L = R6.L;

//A0.H = Dreg_hi ; /* most significant 16 bits of Dreg into most significant 16 bits of A0.W (b) */
A0.H = R0.H;
A0.H = R1.H;
A0.H = R6.H;
//A1.H = Dreg_hi ; /* most significant 16 bits of Dreg into most significant 16 bits of A1.W (b) */
A1.H = R0.H;
A1.H = R1.H;
A1.H = R6.H;

//Dreg_lo = A0 (opt_mode) ; /* move A0 to lower half of Dreg (b) */
R0.L = A0;
R1.L = A0;

R0.L = A0(FU);
R1.L = A0(FU);

R0.L = A0(IS);
R1.L = A0(IS);

R0.L = A0(IU);
R1.L = A0(IU);

R0.L = A0(T);
R1.L = A0(T);

R0.L = A0(S2RND);
R1.L = A0(S2RND);

R0.L = A0(ISS2);
R1.L = A0(ISS2);

R0.L = A0(IH);
R1.L = A0(IH);

//Dreg_hi = A1 (opt_mode) ; /* move A1 to upper half of Dreg (b) */
R0.H = A1;
R1.H = A1;

R0.H = A1(FU);
R1.H = A1(FU);

R0.H = A1(IS);
R1.H = A1(IS);

R0.H = A1(IU);
R1.H = A1(IU);

R0.H = A1(T);
R1.H = A1(T);

R0.H = A1(S2RND);
R1.H = A1(S2RND);

R0.H = A1(ISS2);
R1.H = A1(ISS2);

R0.H = A1(IH);
R1.H = A1(IH);


//Dreg_lo = A0, Dreg_hi = A1 (opt_mode) ; /* move both values at once; must go to the lower and upper halves of the same Dreg (b)*/

R0.L = A0, R0.H = A1; 
R1.L = A0, R1.H = A1; 

R0.L = A0, R0.H = A1(FU); 
R1.L = A0, R1.H = A1(FU);
 
R0.L = A0, R0.H = A1(IS); 
R1.L = A0, R1.H = A1(IS);
 
R0.L = A0, R0.H = A1(IU); 
R1.L = A0, R1.H = A1(IU);
 
R0.L = A0, R0.H = A1(T); 
R1.L = A0, R1.H = A1(T);
 
R0.L = A0, R0.H = A1(S2RND); 
R1.L = A0, R1.H = A1(S2RND);
 
R0.L = A0, R0.H = A1(ISS2); 
R1.L = A0, R1.H = A1(ISS2);

R0.L = A0, R0.H = A1(IH); 
R1.L = A0, R1.H = A1(IH);
 
//Dreg_hi = A1, Dreg_lo = AO (opt_mode) ; /* move both values at once; must go to the upper and lower halves of the same Dreg (b) */

R0.H = A1,R0.L = A0;  
R1.H = A1,R1.L = A0;  
		            
R0.H = A1,R0.L = A0 (FU); 
R1.H = A1,R1.L = A0 (FU);
		            
R0.H = A1,R0.L = A0 (IS); 
R1.H = A1,R1.L = A0 (IS);
		            
R0.H = A1,R0.L = A0 (IU); 
R1.H = A1,R1.L = A0 (IU);
		            
R0.H = A1,R0.L = A0 (T); 
R1.H = A1,R1.L = A0 (T);
		            
R0.H = A1,R0.L = A0 (S2RND); 
R1.H = A1,R1.L = A0 (S2RND);
		            
R0.H = A1,R0.L = A0 (ISS2); 
R1.H = A1,R1.L = A0 (ISS2);
		            
R0.H = A1,R0.L = A0 (IH); 
R1.H = A1,R1.L = A0 (IH);
		            
//Dreg = Dreg_byte (Z) ; /* (a)*/

R0 = R1.B(Z);
R0 = R2.B(Z);

R7 = R1.B(Z);
R7 = R2.B(Z);

//Dreg = Dreg_byte (X) ; /* (a) */
R0 = R1.B(X);
R0 = R2.B(X);

R7 = R1.B(X);
R7 = R2.B(X);

