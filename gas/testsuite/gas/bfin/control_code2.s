
.EXTERN MY_LABEL2;
.section .text;

//
//6 CONTROL CODE BIT MANAGEMENT
//

//CC = Dreg == Dreg ; /* equal, register, signed (a) */
CC = R7 == R0;
CC = R6 == R1;
CC = R0 == R7;

//CC = Dreg == imm3 ; /* equal, immediate, signed (a) */
CC = R7 == -4;
CC = R7 == 3;
CC = R0 == -4;
CC = R0 == 3;

//CC = Dreg < Dreg ; /* less than, register, signed (a) */
CC = R7 < R0;
CC = R6 < R0;
CC = R7 < R1;
CC = R1 < R7;
CC = R0 < R6;

//CC = Dreg < imm3 ; /* less than, immediate, signed (a) */
CC = R7 < -4;
CC = R6 < -4;
CC = R7 < 3;
CC = R1 < 3;

//CC = Dreg <= Dreg ; /* less than or equal, register, signed (a) */
CC = R7 <= R0;
CC = R6 <= R0;
CC = R7 <= R1;
CC = R1 <= R7;
CC = R0 <= R6;

//CC = Dreg <= imm3 ; /* less than or equal, immediate, signed (a) */
CC = R7 <= -4;
CC = R6 <= -4;
CC = R7 <= 3;
CC = R1 <= 3;

//CC = Dreg < Dreg (IU) ; /* less than, register, unsigned (a) */
CC = R7 < R0(IU);
CC = R6 < R0(IU);
CC = R7 < R1(IU);
CC = R1 < R7(IU);
CC = R0 < R6(IU);

//CC = Dreg < uimm3 (IU) ; /* less than, immediate, unsigned (a) */
CC = R7 < 0(IU);
CC = R6 < 0(IU);
CC = R7 < 7(IU);
CC = R1 < 7(IU);
//CC = Dreg <= Dreg (IU) ; /* less than or equal, register, unsigned (a) */
CC = R7 <= R0(IU);
CC = R6 <= R0(IU);
CC = R7 <= R1(IU);
CC = R1 <= R7(IU);
CC = R0 <= R6(IU);


//CC = Dreg <= uimm3 (IU) ; /* less than or equal, immediate unsigned (a) */
CC = R7 <= 0(IU);
CC = R6 <= 0(IU);
CC = R7 <= 7(IU);
CC = R1 <= 7(IU);

//CC = Preg == Preg ; /* equal, register, signed (a) */
CC = P5 == P0;
CC = P5 == P1;
CC = P0 == P2;
CC = P3 == P5;

//CC = Preg == imm3 ; /* equal, immediate, signed (a) */
CC = P5 == -4;
CC = P5 == 0;
CC = P5 == 3;
CC = P2 == -4;
CC = P2 == 0;
CC = P2 == 3;

//CC = Preg < Preg ; /* less than, register, signed (a) */
CC = P5 < P0;
CC = P5 < P1;
CC = P0 < P2;
CC = P3 < P5;

//CC = Preg < imm3 ; /* less than, immediate, signed (a) */
CC = P5 < -4;
CC = P5 < 0;
CC = P5 < 3;
CC = P2 < -4;
CC = P2 < 0;
CC = P2 < 3;


//CC = Preg <= Preg ; /* less than or equal, register, signed (a) */
CC = P5 <= P0;
CC = P5 <= P1;
CC = P0 <= P2;
CC = P3 <= P5;

//CC = Preg <= imm3 ; /* less than or equal, immediate, signed (a) */
CC = P5 <= -4;
CC = P5 <= 0;
CC = P5 <= 3;
CC = P2 <= -4;
CC = P2 <= 0;
CC = P2 <= 3;

//CC = Preg < Preg (IU) ; /* less than, register, unsigned (a) */
CC = P5 < P0(IU);
CC = P5 < P1(IU);
CC = P0 < P2(IU);
CC = P3 < P5(IU);

//CC = Preg < uimm3 (IU) ; /* less than, immediate, unsigned (a) */
CC = P5 < 0(IU);
CC = P5 < 7(IU);
CC = P2 < 0(IU);
CC = P2 < 7(IU);

//CC = Preg <= Preg (IU) ; /* less than or equal, register, unsigned (a) */
CC = P5 <= P0(IU);
CC = P5 <= P1(IU);
CC = P0 <= P2(IU);
CC = P3 <= P5(IU);

//CC = Preg <= uimm3 (IU) ; /* less than or equal, immediate unsigned (a) */
CC = P5 <= 0(IU);
CC = P5 <= 7(IU);
CC = P2 <= 0(IU);
CC = P2 <= 7(IU);

CC = A0 == A1 ; /* equal, signed (a) */
CC = A0 < A1 ; /* less than, Accumulator, signed (a) */
CC = A0 <= A1 ; /* less than or equal, Accumulator, signed (a) */

//Dreg = CC ; /* CC into 32-bit data register, zero-extended (a) */
R7 = CC;
R0 = CC;

//statbit = CC ; /* status bit equals CC (a) */
AZ = CC;
AN = CC;
AC0= CC;
AC1= CC;
//V  = CC;
VS = CC; 
AV0= CC;
AV0S= CC; 
AV1 = CC; 
AV1S= CC; 
AQ  = CC;
//statbit |= CC ; /* status bit equals status bit OR CC (a) */
AZ |= CC;
AN |= CC;
AC0|= CC;
AC1|= CC;
//V  |= CC;
VS |= CC; 
AV0|= CC;
AV0S|= CC; 
AV1 |= CC; 
AV1S|= CC; 
AQ  |= CC;

//statbit &= CC ; /* status bit equals status bit AND CC (a) */
AZ &= CC;
AN &= CC;
AC0&= CC;
AC1&= CC;
//V  &= CC;
VS &= CC; 
AV0&= CC;
AV0S&= CC; 
AV1 &= CC; 
AV1S&= CC; 
AQ  &= CC;

//statbit ^= CC ; /* status bit equals status bit XOR CC (a) */

AZ ^= CC;
AN ^= CC;
AC0^= CC;
AC1^= CC;
//V  ^= CC;
VS ^= CC; 
AV0^= CC;
AV0S^= CC; 
AV1 ^= CC; 
AV1S^= CC; 
AQ  ^= CC;
//CC = Dreg ; /* CC set if the register is non-zero (a) */
CC = R7;
CC = R6;
CC = R1;
CC = R0;


//CC = statbit ; /* CC equals status bit (a) */
CC = AZ;
CC = AN;
CC = AC0;
CC = AC1;
//CC = V;
CC = VS; 
CC = AV0;
CC = AV0S; 
CC = AV1; 
CC = AV1S; 
CC = AQ;

//CC |= statbit ; /* CC equals CC OR status bit (a) */
CC |= AZ;
CC |= AN;
CC |= AC0;
CC |= AC1;
//CC |= V;
CC |= VS; 
CC |= AV0;
CC |= AV0S; 
CC |= AV1; 
CC |= AV1S; 
CC |= AQ;

//CC &= statbit ; /* CC equals CC AND status bit (a) */
CC &= AZ;
CC &= AN;
CC &= AC0;
CC &= AC1;
//CC &= V;
CC &= VS; 
CC &= AV0;
CC &= AV0S; 
CC &= AV1; 
CC &= AV1S; 
CC &= AQ;

//CC ^= statbit ; /* CC equals CC XOR status bit (a) */
CC ^= AZ;
CC ^= AN;
CC ^= AC0;
CC ^= AC1;
//CC ^= V;
CC ^= VS; 
CC ^= AV0;
CC ^= AV0S; 
CC ^= AV1; 
CC ^= AV1S; 
CC ^= AQ;

CC = ! CC ; /* (a) */
