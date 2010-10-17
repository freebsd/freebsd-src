.global _start
_start:
	 ADD	%1,%31,%1
	 NOP	
	 ADDI	%1,%31,0
	 NOP	
	 ADDIU	%1,%31,0
	 NOP	
	 ADDU	%1,%31,%1
	 NOP	
	 ADO16	%1,%31,%1
	 NOP	
	 AND	%1,%31,%1
	 NOP	
	 ANDI	%1,%31,0
	 NOP	
	 ANDOI	%1,%31,0
	 NOP	
	 ANDOUI	%1,%31,0
	 NOP	
	 LUI	%1,0
	 NOP	
	 MRGB	%1,%31,%1,0
	 NOP	
	 NOR	%1,%31,%1
	 NOP	
	 OR	%1,%31,%1
	 NOP	
	 ORI	%1,%31,0
	 NOP	
	 ORUI	%1,%31,0
	 NOP	
	 SLL	%1,%1,0
	 NOP	
	 SLLV	%1,%1,%31
	 NOP	
	 SLT	%1,%31,%1
	 NOP	
	 SLTI	%1,%31,0
	 NOP	
	 SLTIU	%1,%31,0
	 NOP	
	 SLTU	%1,%31,%1
	 NOP	
	 SRA	%1,%1,0
	 NOP	
	 SRAV	%1,%1,%31
	 NOP	
	 SRL	%1,%1,0
	 NOP	
	 SRLV	%1,%1,%31
	 NOP	
	 SUB	%1,%31,%1
	 NOP	
	 SUBU	%1,%31,%1
	 NOP	
	 XOR	%1,%31,%1
	 NOP	
	 XORI	%1,%31,0
	 NOP	
	 NOP	
	 NOP	
	 SRMV	%1,%31,%1,0
	 NOP	
	 SLMV	%1,%31,%1,0
	 NOP	
	 RAM	%1,%1,0,0,0
	 NOP	
	 BBI	%31(0),_start
	 NOP	
	 BBIN	%31(0),_start
	 NOP	
	 BBV	%31,%1,_start
	 NOP	
	 BBVN	%31,%1,_start
	 NOP	
	 BBIL	%31(0),_start
	 NOP	
	 BBINL	%31(0),_start
	 NOP	
	 BBVL	%31,%1,_start
	 NOP	
	 BBVNL	%31,%1,_start
	 NOP	
	 BEQ	%31,%1,_start
	 NOP	
	 BEQL	%31,%1,_start
	 NOP	
	 BGEZ	%31,_start
	 NOP	
	 BGTZAL	%31,_start
	 NOP	
	 BGEZAL	%31,_start
	 NOP	
	 BGTZALL	%31,_start
	 NOP	
	 BGEZALL	%31,_start
	 NOP	
	 BGEZL	%31,_start
	 NOP	
	 BGTZL	%31,_start
	 NOP	
	 BGTZ	%31,_start
	 NOP	
	 BLEZ	%31,_start
	 NOP	
	 BLEZAL	%31,_start
	 NOP	
	 BLTZ	%31,_start
	 NOP	
	 BLTZAL	%31,_start
	 NOP	
	 BLEZL	%31,_start
	 NOP	
	 BLTZL	%31,_start
	 NOP	
	 BLEZALL	%31,_start
	 NOP	
	 BLTZALL	%31,_start
	 NOP	
	 BMB	%31,%1,_start
	 NOP	
	 BMBL	%31,%1,_start
	 NOP	
	 BMB0	%31,%1,_start
	 NOP	
	 BMB1	%31,%1,_start
	 NOP	
	 BMB2	%31,%1,_start
	 NOP	
	 BMB3	%31,%1,_start
	 NOP	
	 BNE	%31,%1,_start
	 NOP	
	 BNEL	%31,%1,_start
	 NOP	
	 J	0
	 NOP	
	 JAL	%31,0
	 NOP	
	 JALR	%1,%31
	 NOP	
	 JR	%31
	 NOP	
	 BREAK	
	 NOP	
	 CTC	%31,%1
	 NOP	
	 CFC	%1,%1
	 NOP	
	 LW	%1,0(%31)
	 NOP	
	 LH	%1,0(%31)
	 NOP	
	 LB	%1,0(%31)
	 NOP	
	 LHU	%1,0(%31)
	 NOP	
	 LBU	%1,0(%31)
	 NOP	
	 SB	%1,0(%31)
	 NOP	
	 SH	%1,0(%31)
	 NOP	
	 SW	%1,0(%31)
	 NOP	
	 RBA	%31,%1,%1
	 NOP	
	 RBAR	%31,%1,%1
	 NOP	
	 RBAL	%31,%1,%1
	 NOP	
	 WBA	%31,%1,%1
	 NOP	
	 WBAC	%31,%1,%1
	 NOP	
	 WBAU	%31,%1,%1
	 NOP	
	 RBI	%31,%1,%1,0
	 NOP	
	 RBIR	%31,%1,%1,0
	 NOP	
	 RBIL	%31,%1,%1,0
	 NOP	
	 WBI	%31,%1,%1,0
	 NOP	
	 WBIC	%31,%1,%1,0
	 NOP	
	 WBIU	%31,%1,%1,0
	 NOP	
	 PKRLA	%31,%1,%1
	 NOP	
	 PKRLAH	%31,%1,%1
	 NOP	
	 PKRLAU	%31,%1,%1
	 NOP	
	 PKRLI	%31,%1,%1,0
	 NOP	
	 PKRLIH	%31,%1,%1,0
	 NOP	
	 PKRLIU	%31,%1,%1,0
	 NOP	
	 LOCK	%1,%1
	 NOP	
	 UNLK	%1,%1
	 NOP	
	 SWWR	%31,%1,%1
	 NOP	
	 SWWRU	%31,%1,%1
	 NOP	
	 SWRD	%1,%1
	 NOP	
	 SWRDL	%1,%1
	 NOP	
	 DWRD	%2,%2
	 NOP	
	 DWRDL	%2,%2
	 NOP	
	 CAM36	%1,%31,1,0
	 NOP	
	 CAM72	%1,%31,1,0
	 NOP	
	 CAM144	%1,%31,1,0
	 NOP	
	 CAM288	%1,%31,1,0
	 NOP	
	 CM32AND	%1,%31,%1
	 NOP	
	 CM32ANDN	%1,%31,%1
	 NOP	
	 CM32OR	%1,%31,%1
	 NOP	
	 CM32RA	%1,%31,%1
	 NOP	
	 CM32RD	%1,%1
	 NOP	
	 CM32RI	%1,%1
	 NOP	
	 CM32RS	%1,%31,%1
	 NOP	
	 CM32SA	%1,%31,%1
	 NOP	
	 CM32SD	%1,%1
	 NOP	
	 CM32SI	%1,%1
	 NOP	
	 CM32SS	%1,%31,%1
	 NOP	
	 CM32XOR	%1,%31,%1
	 NOP	
	 CM64CLR	%2,%2
	 NOP	
	 CM64RA	%2,%31,%2
	 NOP	
	 CM64RD	%2,%2
	 NOP	
	 CM64RI	%2,%2
	 NOP	
	 CM64RIA2	%2,%31,%2
	 NOP	
	 CM64RS	%2,%31,%2
	 NOP	
	 CM64SA	%2,%31,%2
	 NOP	
	 CM64SD	%2,%2
	 NOP	
	 CM64SI	%2,%2
	 NOP	
	 CM64SIA2	%2,%31,%2
	 NOP	
	 CM64SS	%2,%31,%2
	 NOP	
	 CM128RIA2	%2,%31,%2
	 NOP	
	 CM128RIA3	%2,%31,%2,0
	 NOP	
	 CM128RIA4	%2,%31,%2,1
	 NOP	
	 CM128SIA2	%2,%31,%2
	 NOP	
	 CM128SIA3	%2,%31,%2,0
	 NOP	
	 CM128SIA4	%1,%31,%1,0
	 NOP	
	 CM128VSA	%1,%31,%1
	 NOP	
	 CRC32	%1,%31,%1
	 NOP	
	 CRC32B	%1,%31,%1
	 NOP	
	 CHKHDR	%1,%1
	 NOP	
	 AVAIL	%1
	 NOP	
	 FREE	%1,%1
	 NOP	
	 TSTOD	%1,%31
	 NOP	
	 YIELD
	 NOP	
	 CMPHDR	%1
	 NOP	
	 MCID	%1,%1
	 NOP	
	 DBA	%31
	 NOP	
	 DBD	%1,%1
	 NOP	
	 DPWT	%1,%1
	 NOP	
