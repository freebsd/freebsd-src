
.EXTERN MY_LABEL2;
.section .text;

//
//11 EXTERNAL EVENT MANAGEMENT
//
IDLE ; /* (a) */
CSYNC ; /* (a) */
SSYNC ; /* (a) */
EMUEXCPT ; /* (a) */

//CLI Dreg ; /* previous state of IMASK moved to Dreg (a) */
CLI R0;
CLI R1;
CLI R2;

//STI Dreg ; /* previous state of IMASK restored from Dreg (a) */
STI R0;
STI R1;
STI R2;

//RAISE uimm4 ; /* (a) */
RAISE 0;
RAISE 4;
RAISE 15;

//EXCPT uimm4 ; /* (a) */
EXCPT 0;
EXCPT 1;
EXCPT 15;

//TESTSET ( Preg ) ; /* (a) */
TESTSET (P0);
TESTSET (P1);
TESTSET (P2);
//TESTSET (SP);
//TESTSET (FP);

NOP ; /* (a) */
MNOP ; /* (b) */
