
	.sdata
shared:	.word	11

	.data
unshared:
	.word	1
	.word	2
	.word	3
	.word	4

	.text
	.ent	func
func:
	.frame $sp,0,$31
	.set noreorder
	.cpload $25		# 0000 lui	gp,hi(_gp_disp)
				# 0004 addiu	gp,gp,lo(_gp_disp)
				# 0008 addu	gp,gp,t9
	.set reorder
	.cprestore 8		# 000c sw	gp,8(sp)
	.cpadd $4		# 0010 addu	a0,a0,gp
	li	$4, 0x12345678	# 0014 lui	a0,0x1234
				# 0018 ori	a0,a0,0x5678
	la	$4, shared	# 001c lw	a0,got(.sdata)(gp)
				# 0020 nop
				# 0024 addiu	a0,a0,lo(shared)
	la	$4, unshared	# 0028 lw	a0,got(.data)(gp)
				# 002c nop
				# 0030 addiu	a0,a0,lo(unshared)
	la	$4, end		# 0034 lw	a0,got(.text)(gp)
				# 0038 nop
				# 003c addiu	a0,a0,lo(end)
	j	end		# 0040 b	end
				# 0044 nop
	jal	end		# 0048 lw	t9,got(.text)(gp)
				# 004c nop
				# 0050 addiu	t9,t9,lo(end)
				# 0054 jalr	t9
				# 0058 nop
				# 005c lw	gp,8(sp)
	lw	$4, shared	# 0060 lw	a0,got(.sdata)(gp)
				# 0064 nop
				# 0068 addiu	a0,a0,lo(shared)
				# 006c lw	a0,(a0)
	lw	$4, unshared	# 0070 lw	a0,got(.data)(gp)
				# 0074 nop
				# 0078 addiu	a0,a0,lo(unshared)
				# 007c lw	a0,(a0)
	lw	$4, end		# 0080 lw	a0,got(.text)(gp)
				# 0084 nop
				# 0088 addiu	a0,a0,lo(end)
				# 008c lw	a0,(a0)
	ld	$4, shared	# 0090 lw	at,got(.sdata)(gp)
				# 0094 nop
				# 0098 lw	a0,lo(shared)(at)
				# 009c lw	a1,lo(shared)+4(at)
	ld	$4, unshared	# 00a0 lw	at,got(.data)(gp)
				# 00a4 nop
				# 00a8 lw	a0,lo(unshared)(at)
				# 00ac lw	a1,lo(unshared)+4(at)
	ld	$4, end		# 00b0 lw	at,got(.text)(gp)
				# 00b4 nop
				# 00b8 lw	a0,lo(end)(at)
				# 00bc lw	a1,lo(end)+4(at)
	sw	$4, shared	# 00c0 lw	at,got(.sdata)(gp)
				# 00c4 nop
				# 00c8 addiu	at,at,lo(shared)
				# 00cc sw	a0,0(at)
	sw	$4, unshared	# 00d0 lw	at,got(.data)(gp)
				# 00d4 nop
				# 00d8 addiu	at,at,lo(unshared)
				# 00dc sw	a0,0(at)
	sd	$4, shared	# 00e0 lw	at,got(.sdata)(gp)
				# 00e4 nop
				# 00e8 sw	a0,lo(shared)(at)
				# 00ec sw	a1,lo(shared)+4(at)
	sd	$4, unshared	# 00f0 lw	at,got(.data)(gp)
				# 00f4 nop
				# 00f8 sw	a0,lo(unshared)(at)
				# 00fc sw	a1,lo(unshared)+4(at)
	ulh	$4, unshared	# 0100 lw	at,got(.data)(gp)
				# 0104 nop
				# 0108 addiu	at,at,lo(unshared)
				# 010c lb	a0,0(at)
				# 0110 lbu	at,1(at)
				# 0114 sll	a0,a0,8
				# 0118 or	a0,a0,at
	ush	$4, unshared	# 011c lw	at,got(.data)(gp)
				# 0120 nop
				# 0124 addiu	at,at,lo(unshared)
				# 0128 sb	a0,0(at)
				# 012c srl	a0,a0,8
				# 0130 sb	a0,1(at)
				# 0134 lbu	at,0(at)
				# 0138 sll	a0,a0,8
				# 013c or	a0,a0,at
	ulw	$4, unshared	# 0140 lw	at,got(.data)(gp)
				# 0144 nop
				# 0148 addiu	at,at,lo(unshared)
				# 014c lwl	a0,0(at)
				# 0150 lwr	a0,3(at)
	usw	$4, unshared	# 0154 lw	at,got(.data)(gp)
				# 0158 nop
				# 015c addiu	at,at,lo(unshared)
				# 0160 swl	a0,0(at)
				# 0164 swr	a0,3(at)
	li.d	$4, 1.0		# 0168 lui	a0,0x3ff0
				# 016c move	a1,zero
	li.d	$4, 1.9		# 0170 lw	at,got(.rodata)(gp)
				# 0174 lw	a0,lo(F1.9)(at)
				# 0178 lw	a1,lo(F1.9)+4(at)
	li.d	$f0, 1.0	# 017c lui	at,0x3ff0
				# 0180 mtc1	at,$f1
				# 0184 mtc1	zero,$f0
	li.d	$f0, 1.9	# 0188 lw	at,got(.rodata)(gp)
				# 018c ldc1	$f0,lo(L1.9)(at)
	seq	$4, $5, -100	# 0190 addiu	a0,a1,100
				# 0194 sltiu	a0,a0,1
	sne	$4, $5, -100	# 0198 addiu	a0,a1,100
				# 019c sltu	a0,zero,a0
	move	$4, $5		# 01a0 move	a0,a1

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

	.end	func

end:

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space	8
