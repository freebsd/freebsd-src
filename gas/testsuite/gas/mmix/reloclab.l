GAS for MMIX .*/reloclab\.s 			page 1


   1              	#.*
   2              	#.*
   3              	#.*
   4 0000 F0000000 	Main	JMP foo\+8
   4      FD000000 
   4      FD000000 
   4      FD000000 
   4      FD000000 
   5 0014 F0000004 		JMP here
   6 0018 F4080003 		GETA \$8,here
   7 001c 46630002 		BOD \$99,here
   8 0020 FD000000 		SWYM 0
   9 0024 42DE0000 	here	BZ \$222,bar\+16
   9      FD000000 
   9      FD000000 
   9      FD000000 
   9      FD000000 
   9      FD000000 
  10 003c F4040000 	there	GETA \$4,baz
  10      FD000000 
  10      FD000000 
  10      FD000000 
  11 004c F2070000 		PUSHJ \$7,foobar
  12 0050 F1FFFFFB 		JMP there
  13 0054 F558FFFA 		GETA \$88,there
  14 0058 476FFFF9 		BOD \$111,there
GAS for MMIX .*/reloclab\.s 			page 2


DEFINED SYMBOLS
.*/reloclab\.s:4      \.text:0000000000000000 Main
.*/reloclab\.s:9      \.text:0000000000000024 here
.*/reloclab\.s:10     \.text:000000000000003c there

UNDEFINED SYMBOLS
foo
bar
baz
foobar
