
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
	ld	$4, shared	# 0038 ld	a0,shared(gp)
	ld	$4, unshared	# 003c lui	a0,hi(unshared)
				# 0040 ld	a0,lo(unshared)(a0)
	ld	$4, end		# 0044 lui	a0,hi(end)
				# 0048 ld	a0,lo(end)(a0)
	sw	$4, shared	# 004c sw	a0,shared(gp)
	sw	$4, unshared	# 0050 lui	at,hi(unshared)
				# 0054 sw	a0,lo(unshared)(at)
	sd	$4, shared	# 0058 sd	a0,shared(gp)
	sd	$4, unshared	# 005c lui	at,hi(unshared)
				# 0060 sd	a0,lo(unshared)(at)
	ulh	$4, unshared	# 0064 lui	at,hi(unshared)
				# 0068 addiu	at,at,lo(unshared)
				# 006c lb	a0,0(at)
				# 0070 lbu	at,1(at)
				# 0074 sll	a0,a0,8
				# 0078 or	a0,a0,at
	ush	$4, unshared	# 007c lui	at,hi(unshared)
				# 0080 addiu	at,at,lo(unshared)
				# 0084 sb	a0,1(at)
				# 0088 srl	a0,a0,8
				# 008c sb	a0,0(at)
				# 0090 lbu	at,1(at)
				# 0094 sll	a0,a0,8
				# 0098 or	a0,a0,at
	ulw	$4, unshared	# 009c lui	at,hi(unshared)
				# 00a0 addiu	at,at,lo(unshared)
				# 00a4 lwl	a0,0(at)
				# 00a8 lwr	a0,3(at)
	usw	$4, unshared	# 00ac lui	at,hi(unshared)
				# 00b0 addiu	at,at,lo(unshared)
				# 00b4 swl	a0,0(at)
				# 00b8 swr	a0,3(at)
	li.d	$4, 1.0		# 00bc li	a0,0xffc0
				# 00c0 dsll32	a0,a0,14 # giving 0x3ff00000...
	li.d	$4, 1.9		# 00c4 lui	at,hi(F1.9)
				# 00c8 ld	a0,lo(F1.9)(at)
	li.d	$f0, 1.0	# 00cc lui	at,0x3ff0
				# 00d0 mtc1	at,$f1
				# 00d4 mtc1	zero,$f0
	li.d	$f0, 1.9	# 00d8 ldc1	$f0,L1.9(gp)
	seq	$4, $5, -100	# 00dc daddiu	a0,a1,100
				# 00e0 sltiu	a0,a0,1
	sne	$4, $5, -100	# 00e4 daddiu	a0,a1,100
				# 00e8 sltu	a0,zero,a0
	move	$4, $5		# 00ec move	a0,a1

	dla	$4, shared	# 00f0 addiu	a0,gp,shared
	dla	$4, unshared	# 00f4 lui	a0,hi(unshared)
				# 00f8 addiu	a0,a0,lo(unshared)
	uld	$4, unshared	# 00fc lui	at,hi(unshared)
				# 0100 addiu	at,at,lo(unshared)
				# 0104 ldl	a0,0(at)
				# 0108 ldr	a0,7(at)
	usd	$4, unshared	# 010c lui	at,hi(unshared)
				# 0110 addiu	at,at,lo(unshared)
				# 0114 sdl	a0,0(at)
				# 0118 sdr	a0,7(at)

	bgt	$4, 0x7fffffff, end	# 011c li	at,0x8000
					# 0120 dsll	at,at,0x10
					# 0124 slt	at,a0,at
					# 0128 beqz	at,end
	bgtu	$4, 0xffffffff, end	# 012c li	at,0x8000
					# 0130 dsll	at,at,17
					# 0134 sltu	at,a0,at
					# 0138 beqz	at,end
	ble	$4, 0x7fffffff, end	# 013c li	at,0x8000
					# 0140 dsll	at,at,0x10
					# 0144 slt	at,a0,at
					# 0148 bnez	at,end
	bleu	$4, 0xffffffff, end	# 014c li	at,0x8000
					# 0150 dsll	at,at,17
					# 0154 sltu	at,a0,at
					# 0158 bnez	at,end

# Should produce warnings given -mfp32
#	add.d	$f1, $f2, $f3

end:

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space	8
