 .data
foodata: .word 42
	 .text
footext:

.macro test insn text=""
	.export \insn
\insn:
	\insn \text
.endm

	test abs   r0		
	test addc  "r1,r2"	// A double forward slash starts a line comment
	test addi  "r3, 1"	# So does a hash
	test addu  "r4, r5"	// White space between operands should be ignored
	test and   "r6,r7"   ;	test andi  "r8,#2" // A semicolon seperates statements
	test andn  "r9, r10"
	test asr   "r11, R12"	// Uppercase R is allowed as a register prefix
	test asrc  "r13"
	test asri  "r14,#0x1f"
	test bclri "r15,0"
	test bf    footext
	test bgeni "sp, 7"	// r0 can also be refered to as 'sp'
	test BGENI "r0, 8"	// Officially upper case or mixed case
	test BGENi "r0, 31"	// mnemonics should not be allowed, but we relax this...
	test bgenr "r1, r2"
	test bkpt
	test bmaski "r3,#8"
	test BMASKI "r3,0x1f"
	test br     .  // Dot means the current address
	test brev   r4
	test bseti  "r5,30"
	test bsr    footext
	test bt     footext
	test btsti  "r6, 27"
	test clrc   
	test clrf   r7
	test clrt   r8
	test cmphs  "r9,r10"
	test cmplt  "r11,r12" 
	test cmplei "r11, 14" 
	test cmplti "r13,32"
	test cmpne  "r14, r15"
	test cmpnei "r0,0"
	test decf   r1
	test decgt  r2
	test declt  r3
	test decne  r4
	test dect   r5
	test divs   "r6,r1"
	test divu   "r8, r1"
	test doze
	test ff1    r10
	test incf   r11
	test inct   r12
	test ixh    "r13,r14"
	test ixw    "r15,r0"
	test jbf    footext
	test jbr    fooloop
	test jbsr   footext
	test jbt    fooloop
	test jmp    r1
	test jmpi   footext 
	test jsr    r2
	test jsri   footext
	test ld.b   "r3,(r4,0)"
	test ld.h   "r5 , ( r6, #2)"
	test ld.w   "r7, (r8, 0x4)"
	test ldb    "r9,(r10,#0xf)"
	test ldh    "r11, (r12, 30)"
	test ld     "r13, (r14, 20)"
	test ldw    "r13, (r14, 60)"
	test ldm    "r2-r15,(r0)"
	.export fooloop
fooloop:		
	test ldq    "r4-r7,(r1)"
	test loopt  "r8, fooloop"
	test LRW    "r9, [foolit]"
	test lrw    "r9, 0x4321"   	// PC rel indirect
	.global foolit
foolit:
	.word 0x1234
	test lsl    "r10,r11"
	test lslc   r12
	.literals			// Dump literals table	
	test lsli   "r13,31"
	test lsr    "r14,r15"
	test lsrc   r0
	test lsri   "r1,1"
	test mclri  "r4, 64"
	test mfcr   "r2, cr0"
	test mov    "r3,r4"
	test movf   "r5, r6"
	test movi   "r7, 127"
	test movt   "r8, r9"
	test mtcr   "r10, psr"
	test mult   "r11, r12"
	test mvc    r13
	test mvcv   r14
	test neg    r2
	test not    r15
	test or     "r0,r1"
	test rfi
	test rolc   "r6, 1"
	test rori   "r9, 6"
	test rotlc  "r6, 1"
	test rotli  "r2, #10"
	test rotri  "r9, 6"
	test rsub   "r3, r4"
	test rsubi  "r5, 0x0"
	test rte
	test rts
	test setc
	test sextb  r6
	test sexth  r7
	test st.b   "r8, (r9, 0)"
	test st.h   "r10, (r11, 2)"
	test st.w   "r12, (r13, 4)"
	test stb    "r14, (r15, 15)"	
	test sth    "r0, (r1, 30)"
	test stw    "r2, (r3, 0x3c)"
	test st     "r4, (r5, 0)"
	test stm    "r14 - r15 , (r0)"
	test stop
	test stq    "r4 - r7 , (r1)"
	test subc   "r7, r13"
	test subi   "r14, 32"
	test subu   "r9, r3"
	test sync
	test tstlt  r5
	test tstne  r7
	test trap   2
	test tst    "r14, r14"
	test tstnbz r2
	test wait
	test xor    "r15,r0"
	test xsr    r11
	test xtrb0  "r1, r1"
	test xtrb1  "r1, r2"
	test xtrb2  "r1, r0"
	test xtrb3  "r1, r13"
	test zextb  r8
	test zexth  r4
	clrc			// These two instructions pad the object file
	clrc			// out to a 16 byte boundary.
	