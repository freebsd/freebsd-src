;# bits.s
;# checks all the bit operations in MAXQ10

.text
foo:
 MOVE C, ACC.0	
 MOVE C, ACC.1
 MOVE C, ACC.2
 MOVE C, ACC.3
 MOVE C, ACC.4
 MOVE C, ACC.5
 MOVE C, ACC.6
 MOVE C, ACC.7		;8 bits on a MAXQ10 machine
 MOVE C, #0
 MOVE C, #1
 MOVE ACC.0, C	
 MOVE ACC.1, C 
 MOVE ACC.2, C
 MOVE ACC.3, C
 MOVE ACC.4, C
 MOVE ACC.5, C
 MOVE ACC.6, C
 MOVE ACC.7, C		;8 bits on a MAXQ10 machine
 CPL C
 AND ACC.0	;AND with carry
 AND ACC.1	
 AND ACC.2	
 AND ACC.3	
 AND ACC.4	
 AND ACC.5	
 AND ACC.6	
 AND ACC.7	
 OR ACC.0	;OR with carry
 OR ACC.1	
 OR ACC.2	
 OR ACC.3	
 OR ACC.4	
 OR ACC.5	
 OR ACC.6	
 OR ACC.7	
 XOR ACC.0	;XOR with carry
 XOR ACC.1	
 XOR ACC.2	
 XOR ACC.3	
 XOR ACC.4	
 XOR ACC.5	
 XOR ACC.6	
 XOR ACC.7	
 MOVE C, SC.1
 MOVE C, IMR.0
 MOVE C, IC.0
 MOVE C, PSF.0		;move program status flag bit 0
