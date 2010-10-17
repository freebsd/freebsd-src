.global _start
_start:
	 ADD	%1,%30,%31
	 NOP	
	 ADDI	%31,%30,0
	 NOP	
	 ADDIU	%31,%30,0
	 NOP	
	 ADDU	%1,%30,%31
	 NOP	
	 ADO16	%1,%30,%31
	 NOP	
	 AND	%1,%30,%31
	 NOP	
	 ANDI	%31,%30,0
	 NOP	
	 ANDOI	%31,%30,0
	 NOP	
	 ANDOUI	%31,%30,0
	 NOP	
	 LUI	%31,0
	 NOP	
	 MRGB	%1,%30,%31,0
	 NOP	
	 NOR	%1,%30,%31
	 NOP	
	 OR	%1,%30,%31
	 NOP	
	 ORI	%31,%30,0
	 NOP	
	 ORUI	%31,%30,0
	 NOP	
	 SLL	%1,%31,0
	 NOP	
	 SLLV	%1,%31,%30
	 NOP	
	 SLT	%1,%30,%31
	 NOP	
	 SLTI	%31,%30,0
	 NOP	
	 SLTIU	%31,%30,0
	 NOP	
	 SLTU	%1,%30,%31
	 NOP	
	 SRA	%1,%31,0
	 NOP	
	 SRAV	%1,%31,%30
	 NOP	
	 SRL	%1,%31,0
	 NOP	
	 SRLV	%1,%31,%30
	 NOP	
	 SUB	%1,%30,%31
	 NOP	
	 SUBU	%1,%30,%31
	 NOP	
	 XOR	%1,%30,%31
	 NOP	
	 XORI	%31,%30,0
	 NOP	
	 NOP	
	 NOP	
	 SRMV	%1,%30,%31,0
	 NOP	
	 SLMV	%1,%30,%31,0
	 NOP	
	 RAM	%1,%31,0,0,0
	 NOP	
	 BBI	%30(0),_start
	 NOP	
	 BBIN	%30(0),_start
	 NOP	
	 BBV	%30,%31,_start
	 NOP	
	 BBVN	%30,%31,_start
	 NOP	
	 BBIL	%30(0),_start
	 NOP	
	 BBINL	%30(0),_start
	 NOP	
	 BBVL	%30,%31,_start
	 NOP	
	 BBVNL	%30,%31,_start
	 NOP	
	 BEQ	%30,%31,_start
	 NOP	
	 BEQL	%30,%31,_start
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
	 BMB	%30,%31,_start
	 NOP	
	 BMBL	%30,%31,_start
	 NOP	
	 BMB0	%30,%31,_start
	 NOP	
	 BMB1	%30,%31,_start
	 NOP	
	 BMB2	%30,%31,_start
	 NOP	
	 BMB3	%30,%31,_start
	 NOP	
	 BNE	%30,%31,_start
	 NOP	
	 BNEL	%30,%31,_start
	 NOP	
	 J	0
	 NOP	
	 JAL	%30,0
	 NOP	
	 JALR	%1,%30
	 NOP	
	 JR	%30
	 NOP	
	 BREAK	
	 NOP	
	 CTC	%30,%31
	 NOP	
	 CFC	%1,%31
	 NOP	
	 LW	%31,0(%30)
	 NOP	
	 LH	%31,0(%30)
	 NOP	
	 LB	%31,0(%30)
	 NOP	
	 LHU	%31,0(%30)
	 NOP	
	 LBU	%31,0(%30)
	 NOP	
	 SB	%31,0(%30)
	 NOP	
	 SH	%31,0(%30)
	 NOP	
	 SW	%31,0(%30)
	 NOP	
	 RBA	%30,%31,%1
	 NOP	
	 RBAR	%30,%31,%1
	 NOP	
	 RBAL	%30,%31,%1
	 NOP	
	 WBA	%30,%31,%1
	 NOP	
	 WBAC	%30,%31,%1
	 NOP	
	 WBAU	%30,%31,%1
	 NOP	
	 RBI	%30,%31,%1,0
	 NOP	
	 RBIR	%30,%31,%1,0
	 NOP	
	 RBIL	%30,%31,%1,0
	 NOP	
	 WBI	%30,%31,%1,0
	 NOP	
	 WBIC	%30,%31,%1,0
	 NOP	
	 WBIU	%30,%31,%1,0
	 NOP	
	 PKRLA	%30,%31,%1
	 NOP	
	 PKRLAH	%30,%31,%1
	 NOP	
	 PKRLAU	%30,%31,%1
	 NOP	
	 PKRLI	%30,%31,%1,0
	 NOP	
	 PKRLIH	%30,%31,%1,0
	 NOP	
	 PKRLIU	%30,%31,%1,0
	 NOP	
	 LOCK	%31,%1
	 NOP	
	 UNLK	%31,%1
	 NOP	
	 SWWR	%30,%31,%1
	 NOP	
	 SWWRU	%30,%31,%1
	 NOP	
	 SWRD	%1,%31
	 NOP	
	 SWRDL	%1,%31
	 NOP	
	 DWRD	%2,%30
	 NOP	
	 DWRDL	%2,%30
	 NOP	
	 CAM36	%1,%30,4,0
	 NOP	
	 CAM72	%1,%30,4,0
	 NOP	
	 CAM144	%1,%30,4,0
	 NOP	
	 CAM288	%1,%30,4,0
	 NOP	
	 CM32AND	%1,%30,%31
	 NOP	
	 CM32ANDN	%1,%30,%31
	 NOP	
	 CM32OR	%1,%30,%31
	 NOP	
	 CM32RA	%1,%30,%31
	 NOP	
	 CM32RD	%1,%31
	 NOP	
	 CM32RI	%1,%31
	 NOP	
	 CM32RS	%1,%30,%31
	 NOP	
	 CM32SA	%1,%30,%31
	 NOP	
	 CM32SD	%1,%31
	 NOP	
	 CM32SI	%1,%31
	 NOP	
	 CM32SS	%1,%30,%31
	 NOP	
	 CM32XOR	%1,%30,%31
	 NOP	
	 CM64CLR	%2,%30
	 NOP	
	 CM64RA	%2,%30,%30
	 NOP	
	 CM64RD	%2,%30
	 NOP	
	 CM64RI	%2,%30
	 NOP	
	 CM64RIA2	%2,%30,%30
	 NOP	
	 CM64RS	%2,%30,%30
	 NOP	
	 CM64SA	%2,%30,%30
	 NOP	
	 CM64SD	%2,%30
	 NOP	
	 CM64SI	%2,%30
	 NOP	
	 CM64SIA2	%2,%30,%30
	 NOP	
	 CM64SS	%2,%30,%30
	 NOP	
	 CM128RIA2	%2,%30,%30
	 NOP	
	 CM128RIA3	%2,%30,%30,3
	 NOP	
	 CM128RIA4	%2,%30,%30,4
	 NOP	
	 CM128SIA2	%2,%30,%30
	 NOP	
	 CM128SIA3	%2,%30,%30,3
	 NOP	
	 CM128SIA4	%1,%30,%31,4
	 NOP	
	 CM128VSA	%1,%30,%31
	 NOP	
	 CRC32	%1,%30,%31
	 NOP	
	 CRC32B	%1,%30,%31
	 NOP	
	 CHKHDR	%1,%31
	 NOP	
	 AVAIL	%1
	 NOP	
	 FREE	%31,%1
	 NOP	
	 TSTOD	%31,%1
	 NOP	
	 CMPHDR	%1
	 NOP	
	 MCID	%1,%31
	 NOP	
	 DBA	%30
	 NOP	
	 DBD	%31,%1
	 NOP	
	 DPWT	%31,%1
	 NOP	
