.global _start
_start:
	 ADD	%30,%30,%31
	 NOP	
	 ADDI	%31,%30,0
	 NOP	
	 ADDIU	%31,%30,0
	 NOP	
	 ADDU	%30,%30,%31
	 NOP	
	 ADO16	%30,%30,%31
	 NOP	
	 AND	%30,%30,%31
	 NOP	
	 ANDI	%31,%30,0
	 NOP	
	 ANDOI	%31,%30,0
	 NOP	
	 ANDOUI	%31,%30,0
	 NOP	
	 LUI	%31,0
	 NOP	
	 MRGB	%30,%30,%31,0
	 NOP	
	 NOR	%30,%30,%31
	 NOP	
	 OR	%30,%30,%31
	 NOP	
	 ORI	%31,%30,0
	 NOP	
	 ORUI	%31,%30,0
	 NOP	
	 SLL	%30,%31,0
	 NOP	
	 SLLV	%30,%31,%30
	 NOP	
	 SLT	%30,%30,%31
	 NOP	
	 SLTI	%31,%30,0
	 NOP	
	 SLTIU	%31,%30,0
	 NOP	
	 SLTU	%30,%30,%31
	 NOP	
	 SRA	%30,%31,0
	 NOP	
	 SRAV	%30,%31,%30
	 NOP	
	 SRL	%30,%31,0
	 NOP	
	 SRLV	%30,%31,%30
	 NOP	
	 SUB	%30,%30,%31
	 NOP	
	 SUBU	%30,%30,%31
	 NOP	
	 XOR	%30,%30,%31
	 NOP	
	 XORI	%31,%30,0
	 NOP	
	 NOP	
	 NOP	
	 SRMV	%30,%30,%31,0
	 NOP	
	 SLMV	%30,%30,%31,0
	 NOP	
	 RAM	%30,%31,0,0,0
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
	 JALR	%30,%30
	 NOP	
	 JR	%30
	 NOP	
	 BREAK	
	 NOP	
	 CTC	%30,%31
	 NOP	
	 CFC	%30,%31
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
	 RBA	%30,%31,%30
	 NOP	
	 RBAR	%30,%31,%30
	 NOP	
	 RBAL	%30,%31,%30
	 NOP	
	 WBA	%30,%31,%30
	 NOP	
	 WBAC	%30,%31,%30
	 NOP	
	 WBAU	%30,%31,%30
	 NOP	
	 RBI	%30,%31,%30,0
	 NOP	
	 RBIR	%30,%31,%30,0
	 NOP	
	 RBIL	%30,%31,%30,0
	 NOP	
	 WBI	%30,%31,%30,0
	 NOP	
	 WBIC	%30,%31,%30,0
	 NOP	
	 WBIU	%30,%31,%30,0
	 NOP	
	 PKRLA	%30,%31,%30
	 NOP	
	 PKRLAH	%30,%31,%30
	 NOP	
	 PKRLAU	%30,%31,%30
	 NOP	
	 PKRLI	%30,%31,%30,0
	 NOP	
	 PKRLIH	%30,%31,%30,0
	 NOP	
	 PKRLIU	%30,%31,%30,0
	 NOP	
	 LOCK	%31,%30
	 NOP	
	 UNLK	%31,%30
	 NOP	
	 SWWR	%30,%31,%30
	 NOP	
	 SWWRU	%30,%31,%30
	 NOP	
	 SWRD	%30,%31
	 NOP	
	 SWRDL	%30,%31
	 NOP	
	 DWRD	%30,%30
	 NOP	
	 DWRDL	%30,%30
	 NOP	
	 CAM36	%30,%31,5,0
	 NOP	
	 CAM72	%30,%31,5,0
	 NOP	
	 CAM144	%30,%31,5,0
	 NOP	
	 CAM288	%30,%31,5,0
	 NOP	
	 CM32AND	%30,%30,%31
	 NOP	
	 CM32ANDN	%30,%30,%31
	 NOP	
	 CM32OR	%30,%30,%31
	 NOP	
	 CM32RA	%30,%30,%31
	 NOP	
	 CM32RD	%30,%31
	 NOP	
	 CM32RI	%30,%31
	 NOP	
	 CM32RS	%30,%30,%31
	 NOP	
	 CM32SA	%30,%30,%31
	 NOP	
	 CM32SD	%30,%31
	 NOP	
	 CM32SI	%30,%31
	 NOP	
	 CM32SS	%30,%30,%31
	 NOP	
	 CM32XOR	%30,%30,%31
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
	 CM128RIA4	%30,%30,%30,5
	 NOP	
	 CM128SIA2	%30,%30,%30
	 NOP	
	 CM128SIA3	%30,%30,%30,3
	 NOP	
	 CM128SIA4	%30,%30,%31,5
	 NOP	
	 CM128VSA	%30,%30,%31
	 NOP	
	 CRC32	%30,%30,%31
	 NOP	
	 CRC32B	%30,%30,%31
	 NOP	
	 CHKHDR	%30,%31
	 NOP	
	 AVAIL	%30
	 NOP	
	 FREE	%31,%30
	 NOP	
	 TSTOD	%31,%30
	 NOP	
	 CMPHDR	%30
	 NOP	
	 MCID	%30,%31
	 NOP	
	 DBA	%30
	 NOP	
	 DBD	%31,%30
	 NOP	
	 DPWT	%31,%30
	 NOP	
