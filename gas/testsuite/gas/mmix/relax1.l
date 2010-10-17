GAS for MMIX .*/relax1\.s 			page 1


   1              	#.*
   2              	#.*
   3              	#.*
   4 0000 F0020019 	Main	JMP l6
   5 0004 F0020018 	l0	JMP l6
   6 0008 F0020017 	l1	JMP l6
   7 000c F0020016 	l01	JMP l6
   8 0010 F407FFFF 		GETA \$7,nearfar1	% Within reach\.
   9 0014 F2BFFFFF 		PUSHJ \$191,nearfar2	% Within reach\.
  10 0018 F000FFFE 	l2	JMP nearfar2		% Dummy\.
  11 001c 00000000 		\.space 65530\*4,0
  11      00000000 
  11      00000000 
  11      00000000 
  11      00000000 
  11      00000000 
  12 40004 4D480000 		BNP \$72,l0		% Within reach
  13 40008 F5040000 		GETA \$4,l1		% Within reach\.
  14 4000c F3050000 	nearfar1	PUSHJ 5,l01	% Within reach\.
  15 40010 F4090000 	nearfar2	GETA \$9,l1	% Out of reach\.
  15      FD000000 
  15      FD000000 
  15      FD000000 
  16 40020 F20B0000 		PUSHJ \$11,l3		% Out of reach\.
  16      FD000000 
  16      FD000000 
  16      FD000000 
  16      FD000000 
  17 40034 4437FFFF 	l4	BP \$55,l3		% Within reach\.
  18 40038 00000000 		\.space 65533\*4,0
  18      00000000 
  18      00000000 
  18      00000000 
  18      00000000 
  18      00000000 
  19 8002c F1FDFFF7 		JMP l1			% Dummy\.
  20 80030 F1FDFFF5 	l3	JMP l0			% Dummy\.
  21 80034 47580000 		BOD \$88,l4		% Within reach\.
  22 80038 46580000 		BOD \$88,l4		% Out of reach\.
  22      FD000000 
  22      FD000000 
  22      FD000000 
  22      FD000000 
  22      FD000000 
  23 80050 F0000000 		JMP l5			% Out of reach\.
  23      FD000000 
  23      FD000000 
  23      FD000000 
  23      FD000000 
  24 80064 F0FFFFFF 	l6	JMP l5			% Within reach\.
  25 80068 436FFFF2 		BZ \$111,l3		% Dummy\.
  26 8006c 00000000 		\.space \(256\*256\*256-3\)\*4,0
  26      00000000 
  26      00000000 
  26      00000000 
  26      00000000 
  26      00000000 
GAS for MMIX .*/relax1\.s 			page 2


  27 4080060 F000000D 	l5	JMP l8			% Dummy\.
  28 4080064 F1000000 		JMP l6			% Within reach
  29 4080068 F0000000 		JMP l6			% Out of reach\.
  29      FD000000 
  29      FD000000 
  29      FD000000 
  29      FD000000 
  30 408007c 482C0000 		BNN \$44,l9		% Out of reach\.
  30      FD000000 
  30      FD000000 
  30      FD000000 
  30      FD000000 
  30      FD000000 
  31 4080094 482CFFFF 	l8	BNN \$44,l9		% Within reach\.
  32 4080098 F1FFFFF2 		JMP l5			% Dummy\.
  33 408009c F1FFFFF1 		JMP l5			% Dummy\.
  34 40800a0 00000000 		\.space 65531\*4,0
  34      00000000 
  34      00000000 
  34      00000000 
  34      00000000 
  34      00000000 
  35 40c008c F1FEFFF5 	l10	JMP l5			% Dummy\.
  36 40c0090 F0000007 	l9	JMP l11			% Dummy
  37 40c0094 F3210000 	l7	PUSHJ \$33,l8		% Within reach\.
  38 40c0098 F2210000 		PUSHJ \$33,l8		% Out of reach\.
  38      FD000000 
  38      FD000000 
  38      FD000000 
  38      FD000000 
  39 40c00ac F1FEFFED 	l11	JMP l5			% Dummy\.
  40 40c00b0 F1FEFFF9 		JMP l8			% Dummy\.
  41 40c00b4 00000000 		\.space 65534\*4,0
  41      00000000 
  41      00000000 
  41      00000000 
  41      00000000 
  41      00000000 
  42 41000ac F53D0000 		GETA \$61,l11		% Within reach\.
  43 41000b0 F4480000 		GETA \$72,l11		% Out of reach\.
  43      FD000000 
  43      FD000000 
  43      FD000000 
GAS for MMIX .*/relax1\.s 			page 3


DEFINED SYMBOLS
.*/relax1\.s:4      \.text:0000000000000000 Main
.*/relax1\.s:24     \.text:0000000000080064 l6
.*/relax1\.s:5      \.text:0000000000000004 l0
.*/relax1\.s:6      \.text:0000000000000008 l1
.*/relax1\.s:7      \.text:000000000000000c l01
.*/relax1\.s:14     \.text:000000000004000c nearfar1
.*/relax1\.s:15     \.text:0000000000040010 nearfar2
.*/relax1\.s:10     \.text:0000000000000018 l2
.*/relax1\.s:20     \.text:0000000000080030 l3
.*/relax1\.s:17     \.text:0000000000040034 l4
.*/relax1\.s:27     \.text:0000000004080060 l5
.*/relax1\.s:31     \.text:0000000004080094 l8
.*/relax1\.s:36     \.text:00000000040c0090 l9
.*/relax1\.s:35     \.text:00000000040c008c l10
.*/relax1\.s:39     \.text:00000000040c00ac l11
.*/relax1\.s:37     \.text:00000000040c0094 l7

NO UNDEFINED SYMBOLS
