
.EXTERN MY_LABEL2;
.section .text;

//
//5 STACK CONTROL
//

//[ -- SP ] = allreg ; /* predecrement SP (a) */

[--SP ] =  R0;
[--SP ] =  R6;

[--SP ] =  P0;
[--SP ] =  P4;

[--SP ] =  I0;
[--SP ] =  I1;

[--SP ] =  M0;
[--SP ] =  M1;

[--SP ] =  L0;
[--SP ] =  L1;

[--SP ] =  B0;
[--SP ] =  B1;

[--SP ] =  A0.X;
[--SP ] =  A1.X;

[--SP ] =  A0.W;
[--SP ] =  A1.W;

[--SP ] =  ASTAT;
[--SP ] =  RETS;
[--SP ] =  RETI;
[--SP ] =  RETX;
[--SP ] =  RETN;
[--SP ] =  RETE;
[--SP ] =  LC0;
[--SP ] =  LC1;
[--SP ] =  LT0;
[--SP ] =  LT1;
[--SP ] =  LB0;
[--SP ] =  LB1;
[--SP ] =  CYCLES;
[--SP ] =  CYCLES2;
//[--SP ] =  EMUDAT;
[--SP ] =  USP;
[--SP ] =  SEQSTAT;
[--SP ] =  SYSCFG;


//[ -- SP ] = ( R7 : Dreglim , P5 : Preglim ) ; /* Dregs and indexed Pregs (a) */
[--SP ] = ( R7:0, P5:0);


//[ -- SP ] = ( R7 : Dreglim ) ; /* Dregs, only (a) */
[--SP ] = ( R7:0);

//[ -- SP ] = ( P5 : Preglim ) ; /* indexed Pregs, only (a) */
[--SP ] = (P5:0);


//mostreg = [ SP ++ ] ; /* post-increment SP; does not apply to Data Registers and Pointer Registers (a) */

R0= [ SP ++ ] ;      
R6= [ SP ++ ] ;      
         
P0= [ SP ++ ] ;      
P4= [ SP ++ ] ;      
         
I0= [ SP ++ ] ;      
I1= [ SP ++ ] ;      
         
M0= [ SP ++ ] ;      
M1= [ SP ++ ] ;      
         
L0= [ SP ++ ] ;      
L1= [ SP ++ ] ;      
         
B0= [ SP ++ ] ;      
B1= [ SP ++ ] ;      
         
A0.X= [ SP ++ ] ;    
A1.X= [ SP ++ ] ;    
         
A0.W= [ SP ++ ] ;    
A1.W= [ SP ++ ] ;    
         
ASTAT= [ SP ++ ] ;   
RETS= [ SP ++ ] ;    
RETI= [ SP ++ ] ;    
RETX= [ SP ++ ] ;    
RETN= [ SP ++ ] ;    
RETE= [ SP ++ ] ;    
LC0= [ SP ++ ] ;     
LC1= [ SP ++ ] ;     
LT0= [ SP ++ ] ;     
LT1= [ SP ++ ] ;     
LB0= [ SP ++ ] ;     
LB1= [ SP ++ ] ;     
CYCLES= [ SP ++ ] ;  
CYCLES2= [ SP ++ ] ; 
//EMUDAT= [ SP ++ ] ;  
USP= [ SP ++ ] ;     
SEQSTAT= [ SP ++ ] ; 
SYSCFG= [ SP ++ ] ;  

//( R7 : Dreglim, P5 : Preglim ) = [ SP ++ ] ; /* Dregs and indexed Pregs (a) */
( R7:0, P5:0) = [ SP++ ];

//( R7 : Dreglim ) = [ SP ++ ] ; /* Dregs, only (a) */
( R7:0) = [ SP++ ];

//( P5 : Preglim ) = [ SP ++ ] ; /* indexed Pregs, only (a) */
( P5:0) = [ SP++ ];

//LINK uimm18m4 ; /* allocate a stack frame of specified size (b) */
LINK 0X0;
LINK 0X8;
LINK 0x3FFFC;

UNLINK ; /* de-allocate the stack frame (b)*/
