.global _start
_start:
	 ADD	%30,%30,%30
	 NOP	
	 ADDI	%30,%30,0
	 NOP	
	 ADDIU	%30,%30,0
	 NOP	
	 ADDU	%30,%30,%30
	 NOP	
	 ADO16	%30,%30,%30
	 NOP	
	 AND	%30,%30,%30
	 NOP	
	 ANDI	%30,%30,0
	 NOP	
	 ANDOI	%30,%30,0
	 NOP	
	 ANDOUI	%30,%30,0
	 NOP	
	 LUI	%30,0
	 NOP	
	 MRGB	%30,%30,%30,0
	 NOP	
	 NOR	%30,%30,%30
	 NOP	
	 OR	%30,%30,%30
	 NOP	
	 ORI	%30,%30,0
	 NOP	
	 ORUI	%30,%30,0
	 NOP	
	 SLL	%30,%30,0
	 NOP	
	 SLLV	%30,%30,%30
	 NOP	
	 SLT	%30,%30,%30
	 NOP	
	 SLTI	%30,%30,0
	 NOP	
	 SLTIU	%30,%30,0
	 NOP	
	 SLTU	%30,%30,%30
	 NOP	
	 SRA	%30,%30,0
	 NOP	
	 SRAV	%30,%30,%30
	 NOP	
	 SRL	%30,%30,0
	 NOP	
	 SRLV	%30,%30,%30
	 NOP	
	 SUB	%30,%30,%30
	 NOP	
	 SUBU	%30,%30,%30
	 NOP	
	 XOR	%30,%30,%30
	 NOP	
	 XORI	%30,%30,0
	 NOP	
	 NOP	
	 NOP	
	 SRMV	%30,%30,%30,0
	 NOP	
	 SLMV	%30,%30,%30,0
	 NOP	
	 RAM	%30,%30,0,0,0
	 NOP	
	 BBI	%30(0),_start
	 NOP	
	 BBIN	%30(0),_start
	 NOP	
	 BBV	%30,%30,_start
	 NOP	
	 BBVN	%30,%30,_start
	 NOP	
	 BBIL	%30(0),_start
	 NOP	
	 BBINL	%30(0),_start
	 NOP	
	 BBVL	%30,%30,_start
	 NOP	
	 BBVNL	%30,%30,_start
	 NOP	
	 BEQ	%30,%30,_start
	 NOP	
	 BEQL	%30,%30,_start
	 NOP	
	 BGEZ	%30,_start
	 NOP	
	 BGTZAL	%30,_start
	 NOP	
	 BGEZAL	%30,_start
	 NOP	
	 BGTZALL	%30,_start
	 NOP	
	 BGEZALL	%30,_start
	 NOP	
	 BGEZL	%30,_start
	 NOP	
	 BGTZL	%30,_start
	 NOP	
	 BGTZ	%30,_start
	 NOP	
	 BLEZ	%30,_start
	 NOP	
	 BLEZAL	%30,_start
	 NOP	
	 BLTZ	%30,_start
	 NOP	
	 BLTZAL	%30,_start
	 NOP	
	 BLEZL	%30,_start
	 NOP	
	 BLTZL	%30,_start
	 NOP	
	 BLEZALL	%30,_start
	 NOP	
	 BLTZALL	%30,_start
	 NOP	
	 BMB	%30,%30,_start
	 NOP	
	 BMBL	%30,%30,_start
	 NOP	
	 BMB0	%30,%30,_start
	 NOP	
	 BMB1	%30,%30,_start
	 NOP	
	 BMB2	%30,%30,_start
	 NOP	
	 BMB3	%30,%30,_start
	 NOP	
	 BNE	%30,%30,_start
	 NOP	
	 BNEL	%30,%30,_start
	 NOP	
	 J	0
	 NOP	
	 JAL	%30,0
	 NOP	
	 JALR	%30,%30
	 NOP	
	 JR	%30
	 NOP	
	 BREAK	
	 NOP	
	 CTC	%30,%30
	 NOP	
	 CFC	%30,%30
	 NOP	
	 LW	%30,0(%30)
	 NOP	
	 LH	%30,0(%30)
	 NOP	
	 LB	%30,0(%30)
	 NOP	
	 LHU	%30,0(%30)
	 NOP	
	 LBU	%30,0(%30)
	 NOP	
	 SB	%30,0(%30)
	 NOP	
	 SH	%30,0(%30)
	 NOP	
	 SW	%30,0(%30)
	 NOP	
	 RBA	%30,%30,%30
	 NOP	
	 RBAR	%30,%30,%30
	 NOP	
	 RBAL	%30,%30,%30
	 NOP	
	 WBA	%30,%30,%30
	 NOP	
	 WBAC	%30,%30,%30
	 NOP	
	 WBAU	%30,%30,%30
	 NOP	
	 RBI	%30,%30,%30,0
	 NOP	
	 RBIR	%30,%30,%30,0
	 NOP	
	 RBIL	%30,%30,%30,0
	 NOP	
	 WBI	%30,%30,%30,0
	 NOP	
	 WBIC	%30,%30,%30,0
	 NOP	
	 WBIU	%30,%30,%30,0
	 NOP	
	 PKRLA	%30,%30,%30
	 NOP	
	 PKRLAH	%30,%30,%30
	 NOP	
	 PKRLAU	%30,%30,%30
	 NOP	
	 PKRLI	%30,%30,%30,0
	 NOP	
	 PKRLIH	%30,%30,%30,0
	 NOP	
	 PKRLIU	%30,%30,%30,0
	 NOP	
	 LOCK	%30,%30
	 NOP	
	 UNLK	%30,%30
	 NOP	
	 SWWR	%30,%30,%30
	 NOP	
	 SWWRU	%30,%30,%30
	 NOP	
	 SWRD	%30,%30
	 NOP	
	 SWRDL	%30,%30
	 NOP	
	 DWRD	%30,%30
	 NOP	
	 DWRDL	%30,%30
	 NOP	
	 CAM36	%30,%30,6,0
	 NOP	
	 CAM72	%30,%30,6,0
	 NOP	
	 CAM144	%30,%30,6,0
	 NOP	
	 CAM288	%30,%30,6,0
	 NOP	
	 CM32AND	%30,%30,%30
	 NOP	
	 CM32ANDN	%30,%30,%30
	 NOP	
	 CM32OR	%30,%30,%30
	 NOP	
	 CM32RA	%30,%30,%30
	 NOP	
	 CM32RD	%30,%30
	 NOP	
	 CM32RI	%30,%30
	 NOP	
	 CM32RS	%30,%30,%30
	 NOP	
	 CM32SA	%30,%30,%30
	 NOP	
	 CM32SD	%30,%30
	 NOP	
	 CM32SI	%30,%30
	 NOP	
	 CM32SS	%30,%30,%30
	 NOP	
	 CM32XOR	%30,%30,%30
	 NOP	
	 CM64CLR	%30,%30
	 NOP	
	 CM64RA	%30,%30,%30
	 NOP	
	 CM64RD	%30,%30
	 NOP	
	 CM64RI	%30,%30
	 NOP	
	 CM64RIA2	%30,%30,%30
	 NOP	
	 CM64RS	%30,%30,%30
	 NOP	
	 CM64SA	%30,%30,%30
	 NOP	
	 CM64SD	%30,%30
	 NOP	
	 CM64SI	%30,%30
	 NOP	
	 CM64SIA2	%30,%30,%30
	 NOP	
	 CM64SS	%30,%30,%30
	 NOP	
	 CM128RIA2	%30,%30,%30
	 NOP	
	 CM128RIA3	%30,%30,%30,3
	 NOP	
	 CM128RIA4	%30,%30,%30,6
	 NOP	
	 CM128SIA2	%30,%30,%30
	 NOP	
	 CM128SIA3	%30,%30,%30,3
	 NOP	
	 CM128SIA4	%30,%30,%30,6
	 NOP	
	 CM128VSA	%30,%30,%30
	 NOP	
	 CRC32	%30,%30,%30
	 NOP	
	 CRC32B	%30,%30,%30
	 NOP	
	 CHKHDR	%30,%30
	 NOP	
	 AVAIL	%30
	 NOP	
	 FREE	%30,%30
	 NOP	
	 TSTOD	%30,%30
	 NOP	
	 CMPHDR	%30
	 NOP	
	 MCID	%30,%30
	 NOP	
	 DBA	%30
	 NOP	
	 DBD	%30,%30
	 NOP	
	 DPWT	%30,%30
	 NOP	
