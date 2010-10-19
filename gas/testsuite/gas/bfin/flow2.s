
.EXTERN MY_LABEL2;
.section .text;

//
//2 Program Flow Control
//


//JUMP ( Preg ) ; /* indirect to an absolute (not PC-relative)address (a) */
//Preg: P5-0, SP, FP

JUMP (P0);
JUMP (P1);
JUMP (P2);
JUMP (P3);
JUMP (P4);
JUMP (P5);
JUMP (SP);
JUMP (FP);

//JUMP ( PC + Preg ) ; /* PC-relative, indexed (a) */
JUMP (PC+P0);
JUMP (PC+P1);
JUMP (PC+P2);
JUMP (PC+P3);
JUMP (PC+P4);
JUMP (PC+P5);
JUMP (PC+SP);
JUMP (PC+FP);


//JUMP pcrelm2 ; /* PC-relative, immediate (a) or (b) */

JUMP 0X0;
JUMP 1234;
JUMP -1234;
JUMP 2;
JUMP -2;

MY_LABEL1:
//JUMP.S pcrel13m2 ; /* PC-relative, immediate, short (a) */
JUMP.S 0X0;
JUMP.S 1234;
JUMP.S -1234;
JUMP.S 2;
JUMP.S -2;

//JUMP.L pcrel25m2 ; /* PC-relative, immediate, long (b) */
JUMP.L 0XFF800000;
JUMP.L 0X007FFFFE;
JUMP.L 0X0;
JUMP.L 1234;
JUMP.L -1234;
JUMP.L 2;
JUMP.L -2;

//JUMP user_label ; /* user-defined absolute address label, */
JUMP MY_LABEL1;
JUMP MY_LABEL2;

JUMP MY_LABEL1-2;
JUMP MY_LABEL2-2;

//IF CC JUMP pcrel11m2 ; /* branch if CC=1, branch predicted as not taken (a) */
IF CC JUMP 0xFFFFFE08;
IF CC JUMP 0x0B4;
IF CC JUMP 0;

//IF CC JUMP pcrel11m2 (bp) ; /* branch if CC=1, branch predicted as taken (a) */
IF CC JUMP 0xFFFFFE08(bp);
IF CC JUMP 0x0B4(bp);

//IF !CC JUMP pcrel11m2 ; /* branch if CC=0, branch predicted as not taken (a) */
IF !CC JUMP 0xFFFFFF22;
IF !CC JUMP 0X120;

//IF !CC JUMP pcrel11m2 (bp) ; /* branch if CC=0, branch predicted as taken (a) */
IF !CC JUMP 0xFFFFFF22(bp);
IF !CC JUMP 0X120(bp);

//IF CC JUMP user_label ; /* user-defined absolute address label, resolved by the assembler/linker to the appropriate PC-relative instruction (a) */
IF CC JUMP MY_LABEL1;
IF CC JUMP MY_LABEL2;

//IF CC JUMP user_label (bp) ; /* user-defined absolute address label, resolved by the assembler/linker to the appropriate PC-relative instruction (a) */
IF CC JUMP MY_LABEL1(bp);
IF CC JUMP MY_LABEL2(bp);

//IF !CC JUMP user_label ; /* user-defined absolute address label, resolved by the assembler/linker to the appropriate PC-relative instruction (a) */
IF !CC JUMP MY_LABEL1;
IF !CC JUMP MY_LABEL2;

//IF !CC JUMP user_label (bp) ; /* user-defined absolute address label, resolved by the assembler/linker to the appropriate PC-relative instruction (a) */
IF !CC JUMP MY_LABEL1(bp);
IF !CC JUMP MY_LABEL2(bp);

//CALL ( Preg ) ; /* indirect to an absolute (not PC-relative) address (a) */
CALL(P0);
CALL(P1);
CALL(P2);
CALL(P3);
CALL(P4);
CALL(P5);


//CALL ( PC + Preg ) ; /* PC-relative, indexed (a) */
CALL(PC+P0);
CALL(PC+P1);
CALL(PC+P2);
CALL(PC+P3);
CALL(PC+P4);
CALL(PC+P5);

//CALL pcrel25m2 ; /* PC-relative, immediate (b) */
CALL 0x123456 ;
CALL -1234;

//CALL user_label ; /* user-defined absolute address label,resolved by the assembler/linker to the appropriate PC-relative instruction (a) or (b) */
CALL MY_LABEL1;
CALL MY_LABEL2;

RTS ; // Return from Subroutine (a)
RTI ; // Return from Interrupt (a)
RTX ; // Return from Exception (a)
RTN ; // Return from NMI (a)
RTE ; // Return from Emulation (a)

lsetup ( 4, 4 ) lc0 ;

lsetup ( beg_poll_bit, end_poll_bit ) lc0 ;
NOP;NOP;
beg_poll_bit: R0=1(Z);
end_poll_bit: R1=2(Z);

lsetup ( 4, 6 ) lc1 ;

lsetup ( FIR_filter, bottom_of_FIR_filter ) lc1 ;
NOP;
FIR_filter: R0=1(Z);
bottom_of_FIR_filter: R1=2(Z);

lsetup ( 4, 8 ) lc0 = p1 ;

lsetup ( 4, 8 ) lc0 = p1>>1 ;

loop DoItSome LC0 ; /* define loop DoItSome with Loop Counter 0 */
loop_begin DoItSome ; /* place before the first instruction in the loop */
R0=1;
R1=2;
loop_end DoItSome ; /* place after the last instruction in the loop */

loop DoItSomeMore LC1 ; /* define loop MyLoop with Loop Counter 1*/


