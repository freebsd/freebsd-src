
	.sdata
shared:	.word	11

	.data
unshared:
	.word	1
	.word	2
	.word	3
	.word	4

	.text
func:
	.set noreorder
	li	$4, 0x12345678	# 0000 lui	a0,0x1234
				# 0004 ori	a0,a0,0x5678
	la	$4, shared	# 0008 addiu	a0,gp,shared
	la	$4, unshared	# 000c lui	a0,hi(unshared)
				# 0010 addiu	a0,a0,lo(unshared)
	la	$4, end		# 0014 lui	a0,hi(end)
				# 0018 addiu	a0,a0,lo(end)
	j	end		# 001c j	end
	jal	end		# 0020 jal	end
	lw	$4, shared	# 0024 lw	a0,shared(gp)
	lw	$4, unshared	# 0028 lui	a0,hi(unshared)
				# 002c lw	a0,lo(unshared)(a0)
	lw	$4, end		# 0030 lui	a0,hi(end)
				# 0034 lw	a0,lo(end)(a0)
	ld	$4, shared	# 0038 lw	a0,shared(gp)
				# 003c lw	a1,shared+4(gp)
	ld	$4, unshared	# 0040 lui	at,hi(unshared)
				# 0044 lw	a0,lo(unshared)(at)
				# 0048 lw	a1,lo(unshared)+4(at)
	ld	$4, end		# 004c lui	at,hi(end)
				# 0050 lw	a0,lo(end)(at)
				# 0054 lw	a1,lo(end)+4(at)
	sw	$4, shared	# 0058 sw	a0,shared(gp)
	sw	$4, unshared	# 005c lui	at,hi(unshared)
				# 0060 sw	a0,lo(unshared)(at)
	sd	$4, shared	# 0064 sw	a0,shared(gp)
				# 0068 sw	a1,shared+4(gp)
	sd	$4, unshared	# 006c lui	at,hi(unshared)
				# 0070 sw	a0,lo(unshared)(at)
				# 0074 sw	a1,lo(unshared)+4(at)
	ulh	$4, unshared	# 0078 lui	at,hi(unshared)
				# 007c addiu	at,at,lo(unshared)
				# 0080 lb	a0,0(at)
				# 0084 lbu	at,1(at)
				# 0088 sll	a0,a0,8
				# 008c or	a0,a0,at
	ush	$4, unshared	# 0090 lui	at,hi(unshared)
				# 0094 addiu	at,at,lo(unshared)
				# 0098 sb	a0,1(at)
				# 009c srl	a0,a0,8
				# 00a0 sb	a0,0(at)
				# 00a4 lbu	at,1(at)
				# 00a8 sll	a0,a0,8
				# 00ac or	a0,a0,at
	ulw	$4, unshared	# 00b0 lui	at,hi(unshared)
				# 00b4 addiu	at,at,lo(unshared)
				# 00b8 lwl	a0,0(at)
				# 00bc lwr	a0,3(at)
	usw	$4, unshared	# 00c0 lui	at,hi(unshared)
				# 00c4 addiu	at,at,lo(unshared)
				# 00c8 swl	a0,0(at)
				# 00cc swr	a0,3(at)
	li.d	$4, 1.0		# 00d0 lui	a0,0x3ff0
				# 00d4 move	a1,zero
	li.d	$4, 1.9		# 00d8 lui	at,hi(F1.9)
				# 00dc lw	a0,lo(F1.9)(at)
				# 00e0 lw	a1,lo(F1.9)+4(at)
	li.d	$f0, 1.0	# 00e4 lui	at,0x3ff0
				# 00e8 mtc1	at,$f1
				# 00ec mtc1	zero,$f0
	li.d	$f0, 1.9	# 00f0 ldc1	$f0,L1.9(gp)
	seq	$4, $5, -100	# 00f4 addiu	a0,a1,100
				# 00f8 sltiu	a0,a0,1
	sne	$4, $5, -100	# 00fc addiu	a0,a1,100
				# 0100 sltu	a0,zero,a0
	move	$4, $5		# 0104 move	a0,a1

# Not available in 32-bit mode
#	dla	$4, shared
#	dla	$4, unshared
#	uld	$4, unshared
#	usd	$4, unshared

# Should produce warnings given -mgp32
#	bgt	$4, 0x7fffffff, end
#	bgtu	$4, 0xffffffff, end
#	ble	$4, 0x7fffffff, end
#	bleu	$4, 0xffffffff, end

# Should produce warnings given -mfp32
#	add.d	$f1, $f2, $f3

end:

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space	8
