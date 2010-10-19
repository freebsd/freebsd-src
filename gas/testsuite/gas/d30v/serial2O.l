.*: Assembler messages:
.*:5: Error: Unable to mix instructions as specified
.*:6: Error: Unable to mix instructions as specified
.*:11: Error: Unable to mix instructions as specified
.*:12: Error: Unable to mix instructions as specified
.*:16: Error: Unable to mix instructions as specified
.*:17: Error: Unable to mix instructions as specified
.*:21: Error: Unable to mix instructions as specified
.*:22: Error: Unable to mix instructions as specified
.*:26: Error: Unable to mix instructions as specified
.*:27: Error: Unable to mix instructions as specified
GAS LISTING .*


   1              	# D30V serial execution test
   2              		
   3              		.text
   4              		
   5 \?\?\?\? ........ 		bra -3 -> add r3,r0,0	; Invalid
\*\*\*\*  Error:Unable to mix instructions as specified
   5      ........ 
   6 \?\?\?\? 08083000 		bsr -3 -> add r3,r0,0	; Invalid
\*\*\*\*  Error:Unable to mix instructions as specified
   6      00F00000 
   6      002BFFFF 
   6      00F00000 
   7              	
   8 \?\?\?\? 08083000 		bra/tx -3 -> add r3,r0,0 ;       Valid
   8      00F00000 
   8      100BFFFF 
   8      88083000 
   9 \?\?\?\? 102BFFFF 		bsr/tx -3 -> add r3,r0,0 ;       Valid
   9      88083000 
  10              	
  11 \?\?\?\? 002BFFFF 		bsr -3 -> bsr -10	;       Invalid
\*\*\*\*  Error:Unable to mix instructions as specified
  11      00F00000 
  12 \?\?\?\? 002BFFFE 		bsr -3 -> bsr/xt -10    ;       Invalid
\*\*\*\*  Error:Unable to mix instructions as specified
  12      00F00000 
  12      002BFFFF 
  12      00F00000 
  13 \?\?\?\? 302BFFFE 		bsr/tx -3 -> bsr -10    ;       Valid
  13      00F00000 
  13      102BFFFF 
  13      802BFFFE 
  14 \?\?\?\? 102BFFFF 		bsr/tx -3 -> bsr/fx -10 ;       Valid
  14      A02BFFFE 
  15              	
  16 \?\?\?\? 000BFFFF 		bra -3 -> bra  10       ;      Invalid
\*\*\*\*  Error:Unable to mix instructions as specified
  16      00F00000 
  17 \?\?\?\? 00080001 		bra -3 -> bra/tx 10     ;      Invalid
\*\*\*\*  Error:Unable to mix instructions as specified
  17      00F00000 
  17      000BFFFF 
  17      00F00000 
  18 \?\?\?\? 10080001 		bra/tx -3 -> bra 10     ;      Valid
  18      00F00000 
  18      100BFFFF 
  18      80080001 
  19 \?\?\?\? 100BFFFF 		bra/tx -3 -> bra/fx 10  ;      Valid
  19      A0080001 
  20              	
  21 \?\?\?\? 002BFFFF 		bsr -3 -> bra 10        ;      Invalid
\*\*\*\*  Error:Unable to mix instructions as specified
  21      00F00000 
  22 \?\?\?\? 00080001 		bsr -3 -> bra/tx 10     ;      Invalid
\*\*\*\*  Error:Unable to mix instructions as specified
  22      00F00000 
  22      002BFFFF 
GAS LISTING .*


  22      00F00000 
  23 \?\?\?\? 10080001 		bsr/tx -3 -> bra 10     ;      Valid
  23      00F00000 
  23      102BFFFF 
  23      80080001 
  24 \?\?\?\? 102BFFFF 		bsr/tx -3 -> bra/fx 10  ;      Valid
  24      A0080001 
  25              	
  26 \?\?\?\? 000BFFFF 		bra -3 -> bsr 10        ;      Invalid
\*\*\*\*  Error:Unable to mix instructions as specified
  26      00F00000 
  27 \?\?\?\? 00280001 		bra -3 -> bsr/tx 10     ;      Invalid
\*\*\*\*  Error:Unable to mix instructions as specified
  27      00F00000 
  27      000BFFFF 
  27      00F00000 
  28 \?\?\?\? 10280001 		bra/tx -3 -> bsr 10     ;      Valid
  28      00F00000 
  28      100BFFFF 
  28      80280001 
  29 \?\?\?\? 100BFFFF 		bra/tx -3 -> bsr/fx 10  ;      Valid
  29      A0280001 
