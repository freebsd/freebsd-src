.global _start
_start:
	 ADD	%1,%1,%31
	 NOP	
	 ADDI	%31,%1,0
	 NOP	
	 ADDIU	%31,%1,0
	 NOP	
	 ADDU	%1,%1,%31
	 NOP	
	 ADO16	%1,%1,%31
	 NOP	
	 AND	%1,%1,%31
	 NOP	
	 ANDI	%31,%1,0
	 NOP	
	 ANDOI	%31,%1,0
	 NOP	
	 ANDOUI	%31,%1,0
	 NOP	
	 LUI	%31,0
	 NOP	
	 MRGB	%1,%1,%31,0
	 NOP	
	 NOR	%1,%1,%31
	 NOP	
	 OR	%1,%1,%31
	 NOP	
	 ORI	%31,%1,0
	 NOP	
	 ORUI	%31,%1,0
	 NOP	
	 SLL	%1,%31,0
	 NOP	
	 SLLV	%1,%31,%1
	 NOP	
	 SLT	%1,%1,%31
	 NOP	
	 SLTI	%31,%1,0
	 NOP	
	 SLTIU	%31,%1,0
	 NOP	
	 SLTU	%1,%1,%31
	 NOP	
	 SRA	%1,%31,0
	 NOP	
	 SRAV	%1,%31,%1
	 NOP	
	 SRL	%1,%31,0
	 NOP	
	 SRLV	%1,%31,%1
	 NOP	
	 SUB	%1,%1,%31
	 NOP	
	 SUBU	%1,%1,%31
	 NOP	
	 XOR	%1,%1,%31
	 NOP	
	 XORI	%31,%1,0
	 NOP	
	 NOP	
	 NOP	
	 SRMV	%1,%1,%31,0
	 NOP	
	 SLMV	%1,%1,%31,0
	 NOP	
	 RAM	%1,%31,0,0,0
	 NOP	
	 BBI	%1(0),_start
	 NOP	
	 BBIN	%1(0),_start
	 NOP	
	 BBV	%1,%31,_start
	 NOP	
	 BBVN	%1,%31,_start
	 NOP	
	 BBIL	%1(0),_start
	 NOP	
	 BBINL	%1(0),_start
	 NOP	
	 BBVL	%1,%31,_start
	 NOP	
	 BBVNL	%1,%31,_start
	 NOP	
	 BEQ	%1,%31,_start
	 NOP	
	 BEQL	%1,%31,_start
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
	 BMB	%1,%31,_start
	 NOP	
	 BMBL	%1,%31,_start
	 NOP	
	 BMB0	%1,%31,_start
	 NOP	
	 BMB1	%1,%31,_start
	 NOP	
	 BMB2	%1,%31,_start
	 NOP	
	 BMB3	%1,%31,_start
	 NOP	
	 BNE	%1,%31,_start
	 NOP	
	 BNEL	%1,%31,_start
	 NOP	
	 J	0
	 NOP	
	 JAL	%1,0
	 NOP	
	 JALR	%1,%1
	 NOP	
	 JR	%1
	 NOP	
	 BREAK	
	 NOP	
	 CTC	%1,%31
	 NOP	
	 CFC	%1,%31
	 NOP	
	 LW	%31,0(%1)
	 NOP	
	 LH	%31,0(%1)
	 NOP	
	 LB	%31,0(%1)
	 NOP	
	 LHU	%31,0(%1)
	 NOP	
	 LBU	%31,0(%1)
	 NOP	
	 SB	%31,0(%1)
	 NOP	
	 SH	%31,0(%1)
	 NOP	
	 SW	%31,0(%1)
	 NOP	
	 RBA	%1,%31,%1
	 NOP	
	 RBAR	%1,%31,%1
	 NOP	
	 RBAL	%1,%31,%1
	 NOP	
	 WBA	%1,%31,%1
	 NOP	
	 WBAC	%1,%31,%1
	 NOP	
	 WBAU	%1,%31,%1
	 NOP	
	 RBI	%1,%31,%1,0
	 NOP	
	 RBIR	%1,%31,%1,0
	 NOP	
	 RBIL	%1,%31,%1,0
	 NOP	
	 WBI	%1,%31,%1,0
	 NOP	
	 WBIC	%1,%31,%1,0
	 NOP	
	 WBIU	%1,%31,%1,0
	 NOP	
	 PKRLA	%1,%31,%1
	 NOP	
	 PKRLAH	%1,%31,%1
	 NOP	
	 PKRLAU	%1,%31,%1
	 NOP	
	 PKRLI	%1,%31,%1,0
	 NOP	
	 PKRLIH	%1,%31,%1,0
	 NOP	
	 PKRLIU	%1,%31,%1,0
	 NOP	
	 LOCK	%31,%1
	 NOP	
	 UNLK	%31,%1
	 NOP	
	 SWWR	%1,%31,%1
	 NOP	
	 SWWRU	%1,%31,%1
	 NOP	
	 SWRD	%1,%31
	 NOP	
	 SWRDL	%1,%31
	 NOP	
	 DWRD	%2,%30
	 NOP	
	 DWRDL	%2,%30
	 NOP	
	 CAM36	%1,%31,3,0
	 NOP	
	 CAM72	%1,%31,3,0
	 NOP	
	 CAM144	%1,%31,3,0
	 NOP	
	 CAM288	%1,%31,3,0
	 NOP	
	 CM32AND	%1,%1,%31
	 NOP	
	 CM32ANDN	%1,%1,%31
	 NOP	
	 CM32OR	%1,%1,%31
	 NOP	
	 CM32RA	%1,%1,%31
	 NOP	
	 CM32RD	%1,%31
	 NOP	
	 CM32RI	%1,%31
	 NOP	
	 CM32RS	%1,%1,%31
	 NOP	
	 CM32SA	%1,%1,%31
	 NOP	
	 CM32SD	%1,%31
	 NOP	
	 CM32SI	%1,%31
	 NOP	
	 CM32SS	%1,%1,%31
	 NOP	
	 CM32XOR	%1,%1,%31
	 NOP	
	 CM64CLR	%2,%30
	 NOP	
	 CM64RA	%2,%2,%30
	 NOP	
	 CM64RD	%2,%30
	 NOP	
	 CM64RI	%2,%30
	 NOP	
	 CM64RIA2	%2,%2,%30
	 NOP	
	 CM64RS	%2,%2,%30
	 NOP	
	 CM64SA	%2,%2,%30
	 NOP	
	 CM64SD	%2,%30
	 NOP	
	 CM64SI	%2,%30
	 NOP	
	 CM64SIA2	%2,%2,%30
	 NOP	
	 CM64SS	%2,%2,%30
	 NOP	
	 CM128RIA2	%2,%2,%30
	 NOP	
	 CM128RIA3	%2,%2,%30,3
	 NOP	
	 CM128RIA4	%2,%2,%30,3
	 NOP	
	 CM128SIA2	%2,%2,%30
	 NOP	
	 CM128SIA3	%2,%2,%30,3
	 NOP	
	 CM128SIA4	%1,%1,%31,3
	 NOP	
	 CM128VSA	%1,%1,%31
	 NOP	
	 CRC32	%1,%1,%31
	 NOP	
	 CRC32B	%1,%1,%31
	 NOP	
	 CHKHDR	%1,%31
	 NOP	
	 AVAIL	%1
	 NOP	
	 FREE	%31,%1
	 NOP	
	 CMPHDR	%1
	 NOP	
	 MCID	%1,%31
	 NOP	
	 DBA	%31
	 NOP	
	 DBD	%31,%1
	 NOP	
	 DPWT	%31,%1
	 NOP	
