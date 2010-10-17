.*: Assembler messages:
.*: Warning: Swapping instruction order
.*: Warning: Executing btst in IU in reverse serial may not work
.*: Warning: Executing bclr in IU may not work in parallel execution
.*: Warning: Executing bset in IU may not work
.*: Warning: Swapping instruction order
GAS LISTING .*


   1              	# bittest.s
   2              	#
   3              	# Bit operation instructions \(BCLR, BNOT, BSET, BTST\) should not be placed in IU.
   4              	# If the user specifically indicates they should be in the IU, GAS will
   5              	# generate warnings. The reason why this is not an error is that those instructions 
   6              	# will fail in IU only occasionally. Thus GAS should pack them in MU for
   7              	# safety, and it just needs to draw attention when a violation is given. 
   8              	
   9              		
  10 0000 00F00000 		nop -> ldw R1, @\(R2,R3\)
  10      84401083 
  11 0008 04406144 	        nop || ldw R6, @\(R5,R4\)
.*  Warning:Swapping instruction order
  11      00F00000 
  12              	        
  13 0010 00F00000 	        nop -> BSET R1, R2, R3 
  13      82201083 
  14 0018 80F00000 	        nop <- BTST F1, R2, R3 
.*  Warning:Executing btst in IU in reverse serial may not work
  14      02001083 
  15 0020 00F00000 	        nop || BCLR R1, R2, R3
.*  Warning:Executing bclr in IU may not work in parallel execution
  15      02301083 
  16 0028 00F00000 	        nop -> BNOT R1, R2, R3
  16      82101083 
  17 0030 02101083 	        BNOT r1, r2, r3 -> nop
  17      80F00000 
  18              	        
  19 0038 047C0105 	        bset r1, r2, r3 || moddec r4, 5
.*  Warning:Swapping instruction order
  19      02201083 
  20              	
  21              	        bset r1, r2, r3
  22 0040 02201083 	        moddec r4, 5
  22      847C0105 
  23              	
  24              	        bset r1, r2, r3
  25 0048 02201083 	        joinll r4, r5, r6
  25      88C04146 
  26              	
  27              	        joinll r4, r5, r6
  28 0050 08C04146 	        bset r1, r2, r3
  28      82201083 
