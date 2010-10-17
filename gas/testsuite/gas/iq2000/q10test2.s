.global _start
_start:
	 ADD	%31,%1,%1
	 NOP	
	 ADDI	%1,%1,0
	 NOP	
	 ADDIU	%1,%1,0
	 NOP	
	 ADDU	%31,%1,%1
	 NOP	
	 ADO16	%31,%1,%1
	 NOP	
	 AND	%31,%1,%1
	 NOP	
	 ANDI	%1,%1,0
	 NOP	
	 ANDOI	%1,%1,0
	 NOP	
	 ANDOUI	%1,%1,0
	 NOP	
	 LUI	%1,0
	 NOP	
	 MRGB	%31,%1,%1,0
	 NOP	
	 NOR	%31,%1,%1
	 NOP	
	 OR	%31,%1,%1
	 NOP	
	 ORI	%1,%1,0
	 NOP	
	 ORUI	%1,%1,0
	 NOP	
	 SLL	%31,%1,0
	 NOP	
	 SLLV	%31,%1,%1
	 NOP	
	 SLT	%31,%1,%1
	 NOP	
	 SLTI	%1,%1,0
	 NOP	
	 SLTIU	%1,%1,0
	 NOP	
	 SLTU	%31,%1,%1
	 NOP	
	 SRA	%31,%1,0
	 NOP	
	 SRAV	%31,%1,%1
	 NOP	
	 SRL	%31,%1,0
	 NOP	
	 SRLV	%31,%1,%1
	 NOP	
	 SUB	%31,%1,%1
	 NOP	
	 SUBU	%31,%1,%1
	 NOP	
	 XOR	%31,%1,%1
	 NOP	
	 XORI	%1,%1,0
	 NOP	
	 NOP	
	 NOP	
	 SRMV	%31,%1,%1,0
	 NOP	
	 SLMV	%31,%1,%1,0
	 NOP	
	 RAM	%31,%1,0,0,0
	 NOP	
	 BBI	%1(0),_start
	 NOP	
	 BBIN	%1(0),_start
	 NOP	
	 BBV	%1,%1,_start
	 NOP	
	 BBVN	%1,%1,_start
	 NOP	
	 BBIL	%1(0),_start
	 NOP	
	 BBINL	%1(0),_start
	 NOP	
	 BBVL	%1,%1,_start
	 NOP	
	 BBVNL	%1,%1,_start
	 NOP	
	 BEQ	%1,%1,_start
	 NOP	
	 BEQL	%1,%1,_start
	 NOP	
	 BGEZ	%1,_start
	 NOP	
	 BGTZAL	%1,_start
	 NOP	
	 BGEZAL	%1,_start
	 NOP	
	 BGTZALL	%1,_start
	 NOP	
	 BGEZALL	%1,_start
	 NOP	
	 BGEZL	%1,_start
	 NOP	
	 BGTZL	%1,_start
	 NOP	
	 BGTZ	%1,_start
	 NOP	
	 BLEZ	%1,_start
	 NOP	
	 BLEZAL	%1,_start
	 NOP	
	 BLTZ	%1,_start
	 NOP	
	 BLTZAL	%1,_start
	 NOP	
	 BLEZL	%1,_start
	 NOP	
	 BLTZL	%1,_start
	 NOP	
	 BLEZALL	%1,_start
	 NOP	
	 BLTZALL	%1,_start
	 NOP	
	 BMB	%1,%1,_start
	 NOP	
	 BMBL	%1,%1,_start
	 NOP	
	 BMB0	%1,%1,_start
	 NOP	
	 BMB1	%1,%1,_start
	 NOP	
	 BMB2	%1,%1,_start
	 NOP	
	 BMB3	%1,%1,_start
	 NOP	
	 BNE	%1,%1,_start
	 NOP	
	 BNEL	%1,%1,_start
	 NOP	
	 J	0
	 NOP	
	 JAL	%1,0
	 NOP	
	 JALR	%31,%1
	 NOP	
	 JR	%1
	 NOP	
	 BREAK	
	 NOP	
	 CTC	%1,%1
	 NOP	
	 CFC	%31,%1
	 NOP	
	 LW	%1,0(%1)
	 NOP	
	 LH	%1,0(%1)
	 NOP	
	 LB	%1,0(%1)
	 NOP	
	 LHU	%1,0(%1)
	 NOP	
	 LBU	%1,0(%1)
	 NOP	
	 SB	%1,0(%1)
	 NOP	
	 SH	%1,0(%1)
	 NOP	
	 SW	%1,0(%1)
	 NOP	
	 RBA	%1,%1,%31
	 NOP	
	 RBAR	%1,%1,%31
	 NOP	
	 RBAL	%1,%1,%31
	 NOP	
	 WBA	%1,%1,%31
	 NOP	
	 WBAC	%1,%1,%31
	 NOP	
	 WBAU	%1,%1,%31
	 NOP	
	 RBI	%1,%1,%31,0
	 NOP	
	 RBIR	%1,%1,%31,0
	 NOP	
	 RBIL	%1,%1,%31,0
	 NOP	
	 WBI	%1,%1,%31,0
	 NOP	
	 WBIC	%1,%1,%31,0
	 NOP	
	 WBIU	%1,%1,%31,0
	 NOP	
	 PKRLA	%1,%1,%31
	 NOP	
	 PKRLAC	%1,%1,%31
	 NOP	
	 PKRLAH	%1,%1,%31
	 NOP	
	 PKRLAU	%1,%1,%31
	 NOP	
	 PKRLI	%1,%1,%31,0
	 NOP	
	 PKRLIC	%1,%1,%31,0
	 NOP	
	 PKRLIH	%1,%1,%31,0
	 NOP	
	 PKRLIU	%1,%1,%31,0
	 NOP	
	 LOCK	%1,%31
	 NOP	
	 UNLK	%1,%31
	 NOP	
	 SWWR	%1,%1,%31
	 NOP	
	 SWWRU	%1,%1,%31
	 NOP	
	 SWRD	%31,%1
	 NOP	
	 SWRDL	%31,%1
	 NOP	
	 DWRD	%30,%2
	 NOP	
	 DWRDL	%30,%2
	 NOP	
	 CAM36	%31,%1,2,0
	 NOP	
	 CAM72	%31,%1,2,0
	 NOP	
	 CAM144	%31,%1,2,0
	 NOP	
	 CAM288	%31,%1,2,0
	 NOP	
	 CM32AND	%31,%1,%1
	 NOP	
	 CM32ANDN	%31,%1,%1
	 NOP	
	 CM32OR	%31,%1,%1
	 NOP	
	 CM32RA	%31,%1,%1
	 NOP	
	 CM32RD	%31,%1
	 NOP	
	 CM32RI	%31,%1
	 NOP	
	 CM32RS	%31,%1,%1
	 NOP	
	 CM32SA	%31,%1,%1
	 NOP	
	 CM32SD	%31,%1
	 NOP	
	 CM32SI	%31,%1
	 NOP	
	 CM32SS	%31,%1,%1
	 NOP	
	 CM32XOR	%31,%1,%1
	 NOP	
	 CM64CLR	%30,%2
	 NOP	
	 CM64RA	%30,%2,%2
	 NOP	
	 CM64RD	%30,%2
	 NOP	
	 CM64RI	%30,%2
	 NOP	
	 CM64RIA2	%30,%2,%2
	 NOP	
	 CM64RS	%30,%2,%2
	 NOP	
	 CM64SA	%30,%2,%2
	 NOP	
	 CM64SD	%30,%2
	 NOP	
	 CM64SI	%30,%2
	 NOP	
	 CM64SIA2	%30,%2,%2
	 NOP	
	 CM64SS	%30,%2,%2
	 NOP	
	 CM128RIA2	%30,%2,%2
	 NOP	
	 CM128RIA3	%30,%2,%2,2
	 NOP	
	 CM128RIA4	%30,%2,%2,2
	 NOP	
	 CM128SIA2	%30,%2,%2
	 NOP	
	 CM128SIA3	%30,%2,%2,2
	 NOP	
	 CM128SIA4	%31,%1,%1,2
	 NOP	
	 CM128VSA	%31,%1,%1
	 NOP	
	 CRC32	%31,%1,%1
	 NOP	
	 CRC32B	%31,%1,%1
	 NOP	
	 CHKHDR	%31,%1
	 NOP	
	 AVAIL	%31
	 NOP	
	 FREE	%31,%1
	 NOP	
	 TSTOD	%31,%1
	 NOP	
	 CMPHDR	%31
	 NOP	
	 MCID	%31,%1
	 NOP	
	 DBA	%31
	 NOP	
	 DBD	%1,%31
	 NOP	
	 DPWT	%1,%31
	 NOP	
