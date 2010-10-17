
	.text
	.ent	func1
func1:
	.frame $sp,0,$31
	.set noreorder
	.cpload $25		# 0000 lui	gp,hi(_gp_disp)
				# 0004 addiu	gp,gp,lo(_gp_disp)
				# 0008 addu	gp,gp,t9
	.set reorder
	.cprestore 8		# 000c sw	gp,8(sp)

	jal	end		# 0010 lw	t9,got(.text)(gp)
				# 0014 nop
				# 0018 addiu	t9,t9,lo(end)
				# 001c jalr	t9
				# 0020 nop
				# 0024 lw	gp,8(sp)

	# Avoid confusion: avoid the 'lw' above being put into the delay
	# slot for the jalr below!
	.set noreorder
	nop			# 0028 nop
	.set reorder

	jal	$25		# 002c jalr	t9
				# 0030 nop
				# 0034 lw	gp,8(sp)
	.end	func1


	.text
	.ent	func2
func2:
	.frame $sp,0,$31
	.set noreorder
	.cpload $25		# 0038 lui	gp,hi(_gp_disp)
				# 003c addiu	gp,gp,lo(_gp_disp)
				# 0040 addu	gp,gp,t9
	.set reorder
	.cprestore 32768	# 0044 lui	at,0x1
				# 0048 addu	at,at,sp
				# 004c sw	gp,-32768(at)

	jal	end		# 0050 lw	t9,got(.text)(gp)
				# 0054 nop
				# 0058 addiu	t9,t9,lo(end)
				# 005c jalr	t9
				# 0060 nop
				# 0064 lui	at,0x1
				# 0068 addu	at,at,sp
				# 006c lw	gp,-32768(at)

	# Avoid confusion: avoid the 'lw' above being put into the delay
	# slot for the jalr below!
	.set noreorder
	nop			# 0070 nop
	.set reorder

	jal	$25		# 0074 jalr	t9
				# 0078 nop
				# 007c lui	at,0x1
				# 0080 addu	at,at,sp
				# 0084 lw	gp,-32768(at)
	.end	func2


	.text
	.ent	func3
func3:
	.frame $sp,0,$31
	.set noreorder
	.cpload $25		# 0088 lui	gp,hi(_gp_disp)
				# 008c addiu	gp,gp,lo(_gp_disp)
				# 0090 addu	gp,gp,t9
	.set reorder
	.cprestore 65536	# 0094 lui	at,0x1
				# 0098 addu	at,at,sp
				# 009c sw	gp,0(at)

	jal	end		# 00a0 lw	t9,got(.text)(gp)
				# 00a4 nop
				# 00a8 addiu	t9,t9,lo(end)
				# 00ac jalr	t9
				# 00b0 nop
				# 00b4 lui	at,0x1
				# 00b8 addu	at,at,sp
				# 00bc lw	gp,0(at)

	# Avoid confusion: avoid the 'lw' above being put into the delay
	# slot for the jalr below!
	.set noreorder
	nop			# 00c0 nop
	.set reorder

	jal	$25		# 00c4 jalr	t9
				# 00c8 nop
				# 00cc lui	at,0x1
				# 00d0 addu	at,at,sp
				# 00d4 lw	gp,0(at)

	.end	func3

end:

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space	8
