;# jump.s 
;# Program flow instructions using JUMP
.text
LableStart:	
		JUMP LableStart
		JUMP C, LableStart
		JUMP C, A[0]
		JUMP C, A[1]
		JUMP NC, LableStart
		JUMP NC, A[0]
		JUMP NC, A[1]
		JUMP S, LableStart
		JUMP S, A[0]
		JUMP S, A[1]	
		JUMP Z, LableStart
		JUMP Z, A[0]
		JUMP Z, A[1]	
		JUMP NZ, LableStart
		JUMP NZ, A[0]
		JUMP NZ, A[1]
		JUMP E, LableStart
		JUMP NE, LableStart
		JUMP NE, Lable1

Lable1:			
		SJUMP Lable1		;Checking the SJUMP opcode
		SJUMP C, Lable1
		SJUMP C, A[0]
		SJUMP C, A[1]
		SJUMP NC, Lable1
		SJUMP NC, A[0]
		SJUMP NC, A[1]
		SJUMP S, Lable1
		SJUMP S, A[0]
		SJUMP S, A[1]	
		SJUMP Z, Lable1
		SJUMP Z, A[0]
		SJUMP Z, A[1]	
		SJUMP NZ, Lable1
		SJUMP NZ, A[0]
		SJUMP NZ, A[1]	
		SJUMP E, Lable1
		SJUMP NE, Lable1
		JUMP LongJump
		JUMP C, LongJump
		JUMP C, A[0]
		JUMP C, A[1]
		JUMP NC, LongJump
		JUMP NC, A[0]
		JUMP NC, A[1]
		JUMP Z, LongJump
		JUMP Z, A[0]
		JUMP Z, A[1]
		JUMP NZ, LongJump
		JUMP NZ, A[0]
		JUMP NZ, A[1]
		JUMP S, LongJump
		JUMP S, A[0]
		JUMP S, A[1]	
		JUMP E, LongJump
		JUMP NE, LongJump
		LJUMP LongJump		;test LJUMP also
		LJUMP C, LongJump
		LJUMP C, A[0]
		LJUMP C, A[1]
		LJUMP NE, LongJump
		LJUMP Z, LongJump
		LJUMP Z, A[0]
		LJUMP Z, A[1]
		LJUMP NZ, LongJump
		LJUMP NZ, A[0]
		LJUMP NZ, A[1]
		LJUMP S, LongJump
		LJUMP S, A[0]
		LJUMP S, A[1]
		LJUMP NC, LongJump
		LJUMP NC, A[0]
		LJUMP NC, A[1]	 
		LJUMP E, LongJump
		.fill 0x200, 2, 0 	
LongJump: 
		NOP
		NOP
		NOP
		NOP
		NOP
	
