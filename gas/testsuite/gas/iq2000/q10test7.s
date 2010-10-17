.global _start
_start:
	 ADD	%1,%1,%1
	 NOP	
	 ADDI	%1,%1,8
	 NOP	
	 ADDIU	%1,%1,8
	 NOP	
	 ADDU	%1,%1,%1
	 NOP	
	 ADO16	%1,%1,%1
	 NOP	
	 AND	%1,%1,%1
	 NOP	
	 ANDI	%1,%1,8
	 NOP	
	 ANDOI	%1,%1,8
	 NOP	
	 ANDOUI	%1,%1,8
	 NOP	
	 LUI	%1,8
	 NOP	
	 MRGB	%1,%1,%1,0
	 NOP	
	 NOR	%1,%1,%1
	 NOP	
	 OR	%1,%1,%1
	 NOP	
	 ORI	%1,%1,8
	 NOP	
	 ORUI	%1,%1,8
	 NOP	
	 SLL	%1,%1,0
	 NOP	
	 SLLV	%1,%1,%1
	 NOP	
	 SLT	%1,%1,%1
	 NOP	
	 SLTI	%1,%1,8
	 NOP	
	 SLTIU	%1,%1,8
	 NOP	
	 SLTU	%1,%1,%1
	 NOP	
	 SRA	%1,%1,0
	 NOP	
	 SRAV	%1,%1,%1
	 NOP	
	 SRL	%1,%1,0
	 NOP	
	 SRLV	%1,%1,%1
	 NOP	
	 SUB	%1,%1,%1
	 NOP	
	 SUBU	%1,%1,%1
	 NOP	
	 XOR	%1,%1,%1
	 NOP	
	 XORI	%1,%1,8
	 NOP	
	 NOP	
	 NOP	
	 SRMV	%1,%1,%1,0
	 NOP	
	 SLMV	%1,%1,%1,0
	 NOP	
	 RAM	%1,%1,0,0,0
	 NOP	
	 BBI	%1(1),8
	 NOP	
	 BBIN	%1(1),8
	 NOP	
	 BBV	%1,%1,8
	 NOP	
	 BBVN	%1,%1,8
	 NOP	
	 BBIL	%1(1),8
	 NOP	
	 BBINL	%1(1),8
	 NOP	
	 BBVL	%1,%1,8
	 NOP	
	 BBVNL	%1,%1,8
	 NOP	
	 BEQ	%1,%1,8
	 NOP	
	 BEQL	%1,%1,8
	 NOP	
	 BGEZ	%1,8
	 NOP	
	 BGTZAL	%1,8
	 NOP	
	 BGEZAL	%1,8
	 NOP	
	 BGTZALL	%1,8
	 NOP	
	 BGEZALL	%1,8
	 NOP	
	 BGEZL	%1,8
	 NOP	
	 BGTZL	%1,8
	 NOP	
	 BGTZ	%1,8
	 NOP	
	 BLEZ	%1,8
	 NOP	
	 BLEZAL	%1,8
	 NOP	
	 BLTZ	%1,8
	 NOP	
	 BLTZAL	%1,8
	 NOP	
	 BLEZL	%1,8
	 NOP	
	 BLTZL	%1,8
	 NOP	
	 BLEZALL	%1,8
	 NOP	
	 BLTZALL	%1,8
	 NOP	
	 BMB	%1,%1,8
	 NOP	
	 BMBL	%1,%1,8
	 NOP	
	 BMB0	%1,%1,8
	 NOP	
	 BMB1	%1,%1,8
	 NOP	
	 BMB2	%1,%1,8
	 NOP	
	 BMB3	%1,%1,8
	 NOP	
	 BNE	%1,%1,8
	 NOP	
	 BNEL	%1,%1,8
	 NOP	
	 J	8
	 NOP	
	 JAL	%1,8
	 NOP	
	 JALR	%1,%1
	 NOP	
	 JR	%1
	 NOP	
	 BREAK	
	 NOP	
	 CTC	%1,%1
	 NOP	
	 CFC	%1,%1
	 NOP	
	 LW	%1,8(%1)
	 NOP	
	 LH	%1,8(%1)
	 NOP	
	 LB	%1,8(%1)
	 NOP	
	 LHU	%1,8(%1)
	 NOP	
	 LBU	%1,8(%1)
	 NOP	
	 SB	%1,8(%1)
	 NOP	
	 SH	%1,8(%1)
	 NOP	
	 SW	%1,8(%1)
	 NOP	
	 RBA	%1,%1,%1
	 NOP	
	 RBAR	%1,%1,%1
	 NOP	
	 RBAL	%1,%1,%1
	 NOP	
	 WBA	%1,%1,%1
	 NOP	
	 WBAC	%1,%1,%1
	 NOP	
	 WBAU	%1,%1,%1
	 NOP	
	 RBI	%1,%1,%1,0
	 NOP	
	 RBIR	%1,%1,%1,0
	 NOP	
	 RBIL	%1,%1,%1,0
	 NOP	
	 WBI	%1,%1,%1,0
	 NOP	
	 WBIC	%1,%1,%1,0
	 NOP	
	 WBIU	%1,%1,%1,0
	 NOP	
	 PKRLA	%1,%1,%1
	 NOP	
	 PKRLAH	%1,%1,%1
	 NOP	
	 PKRLAU	%1,%1,%1
	 NOP	
	 PKRLI	%1,%1,%1,0
	 NOP	
	 PKRLIH	%1,%1,%1,0
	 NOP	
	 PKRLIU	%1,%1,%1,0
	 NOP	
	 LOCK	%1,%1
	 NOP	
	 UNLK	%1,%1
	 NOP	
	 SWWR	%1,%1,%1
	 NOP	
	 SWWRU	%1,%1,%1
	 NOP	
	 SWRD	%1,%1
	 NOP	
	 SWRDL	%1,%1
	 NOP	
	 DWRD	%2,%2
	 NOP	
	 DWRDL	%2,%2
	 NOP	
	 CM32AND	%1,%1,%1
	 NOP	
	 CM32ANDN	%1,%1,%1
	 NOP	
	 CM32OR	%1,%1,%1
	 NOP	
	 CM32RA	%1,%1,%1
	 NOP	
	 CM32RD	%1,%1
	 NOP	
	 CM32RI	%1,%1
	 NOP	
	 CM32RS	%1,%1,%1
	 NOP	
	 CM32SA	%1,%1,%1
	 NOP	
	 CM32SD	%1,%1
	 NOP	
	 CM32SI	%1,%1
	 NOP	
	 CM32SS	%1,%1,%1
	 NOP	
	 CM32XOR	%1,%1,%1
	 NOP	
	 CM64CLR	%2,%2
	 NOP	
	 CM64RA	%2,%2,%2
	 NOP	
	 CM64RD	%2,%2
	 NOP	
	 CM64RI	%2,%2
	 NOP	
	 CM64RIA2	%2,%2,%2
	 NOP	
	 CM64RS	%2,%2,%2
	 NOP	
	 CM64SA	%2,%2,%2
	 NOP	
	 CM64SD	%2,%2
	 NOP	
	 CM64SI	%2,%2
	 NOP	
	 CM64SIA2	%2,%2,%2
	 NOP	
	 CM64SS	%2,%2,%2
	 NOP	
	 CM128RIA2	%2,%2,%2
	 NOP	
	 CM128RIA4	%2,%2,%2,7
	 NOP	
	 CM128SIA2	%2,%2,%2
	 NOP	
	 CM128SIA4	%1,%1,%1,7
	 NOP	
	 CM128VSA	%1,%1,%1
	 NOP	
	 CRC32	%1,%1,%1
	 NOP	
	 CRC32B	%1,%1,%1
	 NOP	
	 CHKHDR	%1,%1
	 NOP	
	 AVAIL	%1
	 NOP	
	 FREE	%1,%1
	 NOP	
	 CMPHDR	%1
	 NOP	
	 MCID	%1,%1
	 NOP	
	 DBA	%1
	 NOP	
	 DBD	%1,%1
	 NOP	
	 DPWT	%1,%1
	 NOP	
