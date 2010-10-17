GAS for MMIX .*/reloc8\.s 			page 1


   1              	#.*
   2 0000 B9002DEA 	Main	SYNCD foo,\$45,234
   3 0004 372F002A 		NEGU \$47,bar\+48,localsym
   4 0008 FD00B26E 		SWYM baz-2,45678
   5 000c FF000000 		TRIP fee-1,fie\+1,foe\+3
   6 0010 F9000000 		RESUME foobar\+8
   7              	localsym IS 42
GAS for MMIX .*/reloc8\.s 			page 2


DEFINED SYMBOLS
.*/reloc8\.s:2      \.text:0000000000000000 Main
                            \*ABS\*:000000000000002a localsym

UNDEFINED SYMBOLS
foo
bar
baz
fee
fie
foe
foobar
