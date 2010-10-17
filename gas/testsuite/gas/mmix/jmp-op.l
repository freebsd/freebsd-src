GAS for MMIX .*/jmp-op\.s 			page 1


   1              	#.*
   2 0000 FD000000 	Main	SWYM 0,0,0
   3 0004 FD000000 	here	SWYM 0,0,0
   4 0008 F1FFFFFF 		JMP here
   5 000c F0000000 	at	JMP at
   6 0010 F0000004 		JMP there
   7 0014 F1FFFFFC 		JMP here
   8 0018 F0000002 		JMP there
   9 001c F1FFFFFA 		JMP here
  10 0020 FD000000 	there	SWYM 0,0,0
GAS for MMIX .*/jmp-op\.s 			page 2


DEFINED SYMBOLS
.*/jmp-op\.s:2      \.text:0000000000000000 Main
.*/jmp-op\.s:3      \.text:0000000000000004 here
.*/jmp-op\.s:5      \.text:000000000000000c at
.*/jmp-op\.s:10     \.text:0000000000000020 there

NO UNDEFINED SYMBOLS
