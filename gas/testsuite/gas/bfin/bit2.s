
.EXTERN MY_LABEL2;
.section .text;

//
//8 BIT OPERATIONS
//

//BITCLR ( Dreg , uimm5 ) ; /* (a) */
BITCLR ( R7 , 0 ) ;
BITCLR ( R7 , 31 ) ;
BITCLR ( R7 , 15 ) ;
BITCLR ( R1 , 0 ) ;
BITCLR ( R2 , 1 ) ;
BITCLR ( R3 , 19 ) ;

//BITSET ( Dreg , uimm5 ) ; /* (a) */
BITSET ( R7 , 0 ) ;
BITSET ( R7 , 31 ) ;
BITSET ( R7 , 15 ) ;
BITSET ( R1 , 0 ) ;
BITSET ( R2 , 1 ) ;
BITSET ( R3 , 19 ) ;

//BITTGL ( Dreg , uimm5 ) ; /* (a) */
BITTGL ( R7 , 0 ) ;
BITTGL ( R7 , 31 ) ;
BITTGL ( R7 , 15 ) ;
BITTGL ( R1 , 0 ) ;
BITTGL ( R2 , 1 ) ;
BITTGL ( R3 , 19 ) ;

//CC = BITTST ( Dreg , uimm5 ) ; /* set CC if bit = 1 (a)*/
CC = BITTST ( R7 , 0 ) ;
CC = BITTST ( R7 , 31 ) ;
CC = BITTST ( R7 , 15 ) ;
CC = BITTST ( R1 , 0 ) ;
CC = BITTST ( R2 , 1 ) ;
CC = BITTST ( R3 , 19 ) ;

//CC = ! BITTST ( Dreg , uimm5 ) ; /* set CC if bit = 0 (a)*/
CC = !BITTST ( R7 , 0 ) ;
CC = !BITTST ( R7 , 31 ) ;
CC = !BITTST ( R7 , 15 ) ;
CC = !BITTST ( R1 , 0 ) ;
CC = !BITTST ( R2 , 1 ) ;
CC = !BITTST ( R3 , 19 ) ;

//Dreg = DEPOSIT ( Dreg, Dreg ) ; /* no extension (b) */
R7 = DEPOSIT(R0, R1);
R7 = DEPOSIT(R7, R1);
R7 = DEPOSIT(R7, R7);
R1 = DEPOSIT(R0, R1);
R2 = DEPOSIT(R7, R1);
R3 = DEPOSIT(R7, R7);

//Dreg = DEPOSIT ( Dreg, Dreg ) (X) ; /* sign-extended (b) */
R7 = DEPOSIT(R0, R1)(X);
R7 = DEPOSIT(R7, R1)(X);
R7 = DEPOSIT(R7, R7)(X);
R1 = DEPOSIT(R0, R1)(X);
R2 = DEPOSIT(R7, R1)(X);
R3 = DEPOSIT(R7, R7)(X);

//Dreg = EXTRACT ( Dreg, Dreg_lo ) (Z) ; /* zero-extended (b)*/
R7 = EXTRACT(R0, R1.L)(Z);
R7 = EXTRACT(R7, R1.L)(Z);
R7 = EXTRACT(R7, R7.L)(Z);
R1 = EXTRACT(R0, R1.L)(Z);
R2 = EXTRACT(R7, R1.L)(Z);
R3 = EXTRACT(R7, R7.L)(Z);

//Dreg = EXTRACT ( Dreg, Dreg_lo ) (X) ; /* sign-extended (b)*/
R7 = EXTRACT(R0, R1.L)(X);
R7 = EXTRACT(R7, R1.L)(X);
R7 = EXTRACT(R7, R7.L)(X);
R1 = EXTRACT(R0, R1.L)(X);
R2 = EXTRACT(R7, R1.L)(X);
R3 = EXTRACT(R7, R7.L)(X);

//BITMUX ( Dreg , Dreg , A0 ) (ASR) ; /* shift right, LSB is shifted out (b) */
BITMUX(R0, R1, A0)(ASR);
BITMUX(R0, R2, A0)(ASR);
BITMUX(R1, R3, A0)(ASR);
//BITMUX(R0, R0, A0)(ASR);

//BITMUX ( Dreg , Dreg , A0 ) (ASL) ; /* shift left, MSB is shifted out (b) */
//BITMUX(R0, R0, A0)(ASL);
BITMUX(R0, R1, A0)(ASL);
BITMUX(R1, R2, A0)(ASL);

//Dreg_lo = ONES Dreg ; /* (b) */
R0.L = ONES R0;
R0.L = ONES R1;
R1.L = ONES R6;
R2.L = ONES R7;


