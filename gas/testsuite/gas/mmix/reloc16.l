GAS for MMIX .*/reloc16\.s 			page 1


   1              	#.*
   2 0000 E3040000 	Main	SETL \$4,foo
   3 0004 F82D0000 		POP  45,bar\+42
   4 0008 FD2A0000 		SWYM  42,baz-2345
GAS for MMIX .*/reloc16\.s 			page 2


DEFINED SYMBOLS
.*/reloc16\.s:2      \.text:0000000000000000 Main

UNDEFINED SYMBOLS
foo
bar
baz
