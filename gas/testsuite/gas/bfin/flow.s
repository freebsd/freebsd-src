	 .data
foodata:	.word 42
	.text
footext:
	.text
	.global jump
jump:
	jump(P5);
	Jump (pc + p3);
	jUMp (0);
	JumP.l (-16777216);
	jumP.L (0x00fffffe);
	JUMP.s (4094);
	JUMP.L (0X00FF0000);
	jump (footext);

	.text
	.global ccjump
ccjump:
	if cc jump (-1024);
	IF CC JUMP (1022) (BP);
	if !cc jump (0xffffFc00) (Bp);
	if !cc jumP (0x0112);
	if cC JuMp (footext);
	if CC jUmP (footext) (bp);
	if !cc jump (FOOTEXT) (bP);
	if !Cc JUMP (FooText);

	.text
	.global call
call:
	call (P3);
	Call (PC+p2);
	cALL (0xff000000);
	CalL(0x00FFFFFe);
	CAll call_test;


	.text
	.global return
return:
	rts;
	rTi;
	rtX;
	Rtn;
	RTE;

	.text
	
	.text
	.global loop_lc0
loop_lc0:
	loop first_loop lc0;
	Loop_Begin first_loop;
	R0 = [FP+-3604];
	R1 = 9 (X);
	R0 = [FP+-3604];
	P0 = R0;
	P2 = P0 << 2;
	P2 = P2 + FP;
	R0 = -1200 (X);
	P1 = R0;
	P2 = P2 + P1;
	R0 = 0 (X);
	[P2] = R0;
	R0 = [FP+-3604];
	R0 += 1;
	[FP+-3604] = R0;
	LOOP_END first_loop;

	lOOP second_loop Lc0 = P4;
	Loop_Begin second_loop;
	NOP;
	Loop_End second_loop;

	LOOP third_loop lC0 = P1 >> 1;

	Lsetup (4, 2046) Lc0;
	LSETUP(30, 1024) LC0 = P5;
	LSeTuP (30, 4) lc0 = p0 >> 1;


	.global loop_lc1
loop_lc1:
	loop my_loop lc1;
	lOOP other_loop Lc1 = P4;
	LOOP another_loop lC1 = P1 >> 1;

	Lsetup (4, 2046) Lc1;
	LSETUP (30, 1024) LC1 = P5;
	LSeTuP (30, 4) lc1 = p0 >> 1;
	Loop_Begin another_loop;
	R0 = [FP+-3608];
	P0 = R0;
	P2 = P0 << 2;
	P2 = P2 + FP;
	R0 = -3600 (X);
	P0 = R0;
	P1 = P2 + P0;
	R0 = [FP+-3608];
	P0 = R0;
	P2 = P0 << 2;
	P2 = P2 + FP;
	R0 = -1200 (X);
	P0 = R0;
	P2 = P2 + P0;
	R0 = [P2];
	[P1] = R0;
	LOOP_END another_loop;
