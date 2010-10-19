.*: Assembler messages:
.*:6: Error: Unable to mix instructions as specified
.*:7: Error: Unable to mix instructions as specified
.*:8: Error: Unable to mix instructions as specified
.*:9: Error: Unable to mix instructions as specified
GAS LISTING .*


   1              	# serial.s
   2              	#
   3              	# In the following examples, the right-subinstructions
   4              	# will never be executed.  GAS should detect this.
   5              		
   6 \?\?\?\? ........ 		trap r21 -> add r2, r0, r0 ; right instruction will never be executed.
\*\*\*\*  Error:Unable to mix instructions as specified
   6      ........ 
   7 \?\?\?\? 08002000 		dbt     -> add r2, r0, r0               ; ditto
\*\*\*\*  Error:Unable to mix instructions as specified
   7      00F00000 
   7      00B00000 
   7      00F00000 
   8 \?\?\?\? 08002000 		rtd     -> add r2, r0, r0               ; ditto
\*\*\*\*  Error:Unable to mix instructions as specified
   8      00F00000 
   8      00A00000 
   8      00F00000 
   9 \?\?\?\? 08002000 		reit    -> add r2, r0, r0               ; ditto
\*\*\*\*  Error:Unable to mix instructions as specified
   9      00F00000 
   9      00800000 
   9      00F00000 
  10 \?\?\?\? 08002000 		mvtsys psw,  r1 -> add r2, r0, r0       ; OK
  10      00F00000 
  10      00E00040 
  10      88002000 
  11 \?\?\?\? 00E00042 		mvtsys pswh, r1 -> add r2, r0, r0       ; OK
  11      88002000 
  12 \?\?\?\? 00E00041 		mvtsys pswl, r1 -> add r2, r0, r0       ; OK
  12      88002000 
  13 \?\?\?\? 00E00043 		mvtsys f0, r1 -> add r2, r0, r0         ; OK
  13      88002000 
  14 \?\?\?\? 00E0A040 		mvtsys mod_s, r1 -> add r2, r0, r0      ; OK
  14      88002000 
