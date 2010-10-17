.*: Assembler messages:
.*:5: Warning: Odd numbered register used as target of multi-register instruction
.*:6: Warning: Odd numbered register used as target of multi-register instruction
.*:7: Warning: Odd numbered register used as target of multi-register instruction
.*:8: Warning: Odd numbered register used as target of multi-register instruction
.*:9: Warning: Odd numbered register used as target of multi-register instruction
.*:10: Warning: Odd numbered register used as target of multi-register instruction
.*:11: Warning: Odd numbered register used as target of multi-register instruction
.*:12: Warning: Odd numbered register used as target of multi-register instruction
GAS LISTING .*


   1              	# GAS should print a warning  when an odd register is used as a target 
   2              	# of multi-word instructions: ld2w, ld4bh, ld4bhu, ld2h, st2w, st4hb, st2h, 
   3              	# and mulx2h
   4              	
   5 0000 05681000 	st2w r1, @(r0, 0)	||	nop 
.*  Warning:Odd numbered register used as target of multi-register instruction
   5      00F00000 
   6 0008 04681000 	ld2w r1, @(r0, 0)	||	nop 
.*  Warning:Odd numbered register used as target of multi-register instruction
   6      00F00000 
   7 0010 04581000 	ld4bh r1, @(r0, 0)	||	nop 
.*  Warning:Odd numbered register used as target of multi-register instruction
   7      00F00000 
   8 0018 04D81000 	ld4bhu r1, @(r0, 0)	||	nop 
.*  Warning:Odd numbered register used as target of multi-register instruction
   8      00F00000 
   9 0020 04381000 	ld2h r1, @(r0, 0)	||	nop 
.*  Warning:Odd numbered register used as target of multi-register instruction
   9      00F00000 
  10 0028 05581000 	st4hb r1, @(r0, 0)	||	nop 
.*  Warning:Odd numbered register used as target of multi-register instruction
  10      00F00000 
  11 0030 05381000 	st2h r1, @(r0, 0)	||	nop 
.*  Warning:Odd numbered register used as target of multi-register instruction
  11      00F00000 
  12 0038 00F00000 	nop	||	mulx2h r1, r5, r6   
.*  Warning:Odd numbered register used as target of multi-register instruction
  12      0A101146 
