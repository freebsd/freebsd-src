#  One of the rule on restricted sequence is consecutive IU instruction 
#       IU: MUL, MAC, MACS, MSUB, MSUBS  (a)    
#       IU: MULHXpp, MULX2H, MUL2H      (b)
# This means that instructions in group (a) and in (b) should not be executed
# in IU in consecutive cycles in the order (a)->(b). It does neither prohibit
# executions in the reverse order (b)-> (a) nor consecutive execution of
# group (a)->(a) or (b)->(b)

	mulx2h r5,r6,r7		<-	mulx2h r2,r3,r4
	nop	||	mulx2h r8,r9,r10
	nop	||	mulx2h r11,r12,r13       
	mulx2h r14,r15,r16
	mulx2h r17,r18,r19      
	mulx2h r23,r24,r25     <-	mulx2h r20,r21,r22
	mul    r29,r30,r31     <-	mulx2h r26,r27,r28
	mul    r5, r6, r7      <-	mul r2, r3, r4
	mulx2h r11, r12, r13   <-	mulx2h r8, r9, r10
	mulx2h r17, r18, r19   <-	mul r14, r15, r16
	mul    r23, r24, r25   <-	mulx2h r20, r21, r22
