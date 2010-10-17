# Relaxation border-cases: just-within reach, just-out-of-reach, forward
# and backward.  Have a few variable-length thingies in-between so it
# doesn't get too easy.
Main	JMP l6
l0	JMP l6
l1	JMP l6
l01	JMP l6
	GETA $7,nearfar1	% Within reach.
	PUSHJ $191,nearfar2	% Within reach.
l2	JMP nearfar2		% Dummy.
	.space 65530*4,0
	BNP $72,l0		% Within reach
	GETA $4,l1		% Within reach.
nearfar1	PUSHJ 5,l01	% Within reach.
nearfar2	GETA $9,l1	% Out of reach.
	PUSHJ $11,l3		% Out of reach.
l4	BP $55,l3		% Within reach.
	.space 65533*4,0
	JMP l1			% Dummy.
l3	JMP l0			% Dummy.
	BOD $88,l4		% Within reach.
	BOD $88,l4		% Out of reach.
	JMP l5			% Out of reach.
l6	JMP l5			% Within reach.
	BZ $111,l3		% Dummy.
	.space (256*256*256-3)*4,0
l5	JMP l8			% Dummy.
	JMP l6			% Within reach
	JMP l6			% Out of reach.
	BNN $44,l9		% Out of reach.
l8	BNN $44,l9		% Within reach.
	JMP l5			% Dummy.
	JMP l5			% Dummy.
	.space 65531*4,0
l10	JMP l5			% Dummy.
l9	JMP l11			% Dummy
l7	PUSHJ $33,l8		% Within reach.
	PUSHJ $33,l8		% Out of reach.
l11	JMP l5			% Dummy.
	JMP l8			% Dummy.
	.space 65534*4,0
	GETA $61,l11		% Within reach.
	GETA $72,l11		% Out of reach.
