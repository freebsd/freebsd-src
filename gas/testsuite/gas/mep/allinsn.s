 .data
foodata: .word 42
 .text
footext:
	.text
	.global sb
sb:
	sb $7,($fp)
	sb $5,($9)
	sb $7,($14)
	sb $14,($fp)
	sb $15,($14)
	.text
	.global sh
sh:
	sh $3,($fp)
	sh $12,($1)
	sh $13,($2)
	sh $2,($8)
	sh $12,($10)
	.text
	.global sw
sw:
	sw $11,($0)
	sw $3,($7)
	sw $13,($14)
	sw $8,($9)
	sw $gp,($fp)
	.text
	.global lb
lb:
	lb $12,($11)
	lb $9,($2)
	lb $fp,($11)
	lb $gp,($2)
	lb $2,($12)
	.text
	.global lh
lh:
	lh $15,($8)
	lh $3,($10)
	lh $9,($sp)
	lh $6,($sp)
	lh $15,($11)
	.text
	.global lw
lw:
	lw $12,($10)
	lw $9,($13)
	lw $12,($gp)
	lw $12,($11)
	lw $13,($10)
	.text
	.global lbu
lbu:
	lbu $14,($14)
	lbu $12,($fp)
	lbu $gp,($1)
	lbu $fp,($12)
	lbu $12,($1)
	.text
	.global lhu
lhu:
	lhu $15,($4)
	lhu $14,($4)
	lhu $5,($4)
	lhu $sp,($tp)
	lhu $4,($15)
	.text
	.global sw_sp
sw_sp:
	sw $9,3($8)
	sw $10,4($5)
	sw $0,3($gp)
	sw $0,2($8)
	sw $15,1($8)
	.text
	.global lw_sp
lw_sp:
	lw $tp,1($5)
	lw $15,1($0)
	lw $0,4($12)
	lw $11,1($tp)
	lw $9,3($4)
	.text
	.global sb_tp
sb_tp:
	sb $5,1($1)
	sb $10,1($9)
	sb $5,3($3)
	sb $5,1($3)
	sb $10,4($4)
	.text
	.global sh_tp
sh_tp:
	sh $3,1($0)
	sh $tp,1($9)
	sh $9,4($10)
	sh $15,3($14)
	sh $14,4($9)
	.text
	.global sw_tp
sw_tp:
	sw $6,2($13)
	sw $6,1($15)
	sw $2,2($3)
	sw $6,2($12)
	sw $3,1($11)
	.text
	.global lb_tp
lb_tp:
	lb $tp,4($11)
	lb $13,4($8)
	lb $5,4($5)
	lb $sp,2($gp)
	lb $3,2($3)
	.text
	.global lh_tp
lh_tp:
	lh $7,2($fp)
	lh $4,3($8)
	lh $14,1($sp)
	lh $9,1($0)
	lh $13,2($0)
	.text
	.global lw_tp
lw_tp:
	lw $8,4($15)
	lw $11,4($9)
	lw $gp,1($2)
	lw $9,2($14)
	lw $8,1($12)
	.text
	.global lbu_tp
lbu_tp:
	lbu $12,1($9)
	lbu $11,1($9)
	lbu $14,3($8)
	lbu $0,2($sp)
	lbu $13,1($11)
	.text
	.global lhu_tp
lhu_tp:
	lhu $14,2($10)
	lhu $11,1($8)
	lhu $1,1($0)
	lhu $7,2($15)
	lhu $3,2($tp)
	.text
	.global sb16
sb16:
	sb $7,-1($11)
	sb $tp,1($gp)
	sb $3,1($gp)
	sb $14,2($6)
	sb $14,1($7)
	.text
	.global sh16
sh16:
	sh $12,-1($4)
	sh $sp,1($1)
	sh $2,-2($12)
	sh $9,2($11)
	sh $9,-2($12)
	.text
	.global sw16
sw16:
	sw $11,-1($gp)
	sw $4,4($15)
	sw $2,-2($3)
	sw $6,-1($2)
	sw $fp,-2($tp)
	.text
	.global lb16
lb16:
	lb $10,-2($2)
	lb $3,-2($11)
	lb $12,1($5)
	lb $5,1($5)
	lb $11,2($13)
	.text
	.global lh16
lh16:
	lh $sp,-1($11)
	lh $tp,-2($11)
	lh $2,1($10)
	lh $8,-1($7)
	lh $14,-1($11)
	.text
	.global lw16
lw16:
	lw $0,-1($5)
	lw $12,-2($7)
	lw $1,-2($3)
	lw $1,2($7)
	lw $4,1($fp)
	.text
	.global lbu16
lbu16:
	lbu $12,-1($4)
	lbu $14,1($11)
	lbu $1,-1($13)
	lbu $9,-1($tp)
	lbu $8,1($15)
	.text
	.global lhu16
lhu16:
	lhu $tp,-1($15)
	lhu $gp,2($fp)
	lhu $15,-1($12)
	lhu $3,-1($0)
	lhu $3,-2($12)
	.text
	.global sw24
sw24:
	sw $11,(4)
	sw $sp,(4)
	sw $7,(8)
	sw $10,(16)
	sw $8,(160)
	.text
	.global lw24
lw24:
	lw $4,(4)
	lw $sp,(4)
	lw $4,(16)
	lw $fp,(0)
	lw $tp,(8)
	.text
	.global extb
extb:
	extb $13
	extb $tp
	extb $6
	extb $14
	extb $10
	.text
	.global exth
exth:
	exth $15
	exth $2
	exth $5
	exth $10
	exth $4
	.text
	.global extub
extub:
	extub $2
	extub $tp
	extub $3
	extub $9
	extub $gp
	.text
	.global extuh
extuh:
	extuh $8
	extuh $8
	extuh $4
	extuh $0
	extuh $0
	.text
	.global ssarb
ssarb:
	ssarb 2($fp)
	ssarb 2($13)
	ssarb 1($13)
	ssarb 2($5)
	ssarb 0($9)
	.text
	.global mov
mov:
	mov $2,$3
	mov $3,$11
	mov $15,$10
	mov $15,$0
	mov $3,$tp
	.text
	.global movi8
movi8:
	mov $11,-1
	mov $6,2
	mov $sp,-1
	mov $sp,1
	mov $gp,-1
	.text
	.global movi16
movi16:
	mov $15,0
	mov $0,2
	mov $8,-1
	mov $12,1
	mov $7,-1
	.text
	.global movu24
movu24:
	movu $2,1
	movu $10,4
	movu $9,0
	movu $4,3
	movu $14,1
	.text
	.global movu16
movu16:
	movu $sp,1
	movu $6,3
	movu $0,3
	movu $gp,3
	movu $10,2
	.text
	.global movh
movh:
	movh $8,2
	movh $13,1
	movh $gp,2
	movh $12,0
	movh $11,2
	.text
	.global add3
add3:
	add3 $6,$11,$3
	add3 $14,$13,$5
	add3 $3,$11,$7
	add3 $13,$14,$13
	add3 $0,$14,$8
	.text
	.global add
add:
	add $12,2
	add $12,-1
	add $4,1
	add $6,1
	add $6,2
	.text
	.global add3i
add3i:
	add3 $11,$sp,4
	add3 $4,$sp,1
	add3 $0,$sp,0
	add3 $13,$sp,3
	add3 $11,$sp,0
	.text
	.global advck3
advck3:
	advck3 $0,$gp,$10
	advck3 $0,$tp,$0
	advck3 $0,$gp,$13
	advck3 $0,$7,$fp
	advck3 $0,$1,$2
	.text
	.global sub
sub:
	sub $8,$14
	sub $1,$9
	sub $13,$7
	sub $15,$3
	sub $2,$7
	.text
	.global sbvck3
sbvck3:
	sbvck3 $0,$3,$gp
	sbvck3 $0,$3,$7
	sbvck3 $0,$10,$10
	sbvck3 $0,$4,$tp
	sbvck3 $0,$10,$15
	.text
	.global neg
neg:
	neg $14,$7
	neg $1,$7
	neg $2,$11
	neg $13,$fp
	neg $14,$13
	.text
	.global slt3
slt3:
	slt3 $0,$14,$8
	slt3 $0,$4,$13
	slt3 $0,$10,$14
	slt3 $0,$14,$5
	slt3 $0,$3,$12
	.text
	.global sltu3
sltu3:
	sltu3 $0,$2,$8
	sltu3 $0,$gp,$11
	sltu3 $0,$2,$tp
	sltu3 $0,$9,$fp
	sltu3 $0,$6,$9
	.text
	.global slt3i
slt3i:
	slt3 $0,$6,2
	slt3 $0,$11,1
	slt3 $0,$15,0
	slt3 $0,$3,0
	slt3 $0,$tp,0
	.text
	.global sltu3i
sltu3i:
	sltu3 $0,$14,4
	sltu3 $0,$tp,3
	sltu3 $0,$3,1
	sltu3 $0,$12,0
	sltu3 $0,$1,3
	.text
	.global sl1ad3
sl1ad3:
	sl1ad3 $0,$fp,$gp
	sl1ad3 $0,$4,$2
	sl1ad3 $0,$sp,$12
	sl1ad3 $0,$9,$1
	sl1ad3 $0,$fp,$2
	.text
	.global sl2ad3
sl2ad3:
	sl2ad3 $0,$8,$13
	sl2ad3 $0,$2,$3
	sl2ad3 $0,$8,$9
	sl2ad3 $0,$7,$12
	sl2ad3 $0,$4,$12
	.text
	.global add3x
add3x:
	add3 $tp,$11,1
	add3 $tp,$4,-1
	add3 $2,$13,1
	add3 $3,$gp,1
	add3 $10,$15,2
	.text
	.global slt3x
slt3x:
	slt3 $fp,$1,-1
	slt3 $0,$3,-2
	slt3 $9,$15,-1
	slt3 $3,$fp,2
	slt3 $tp,$14,0
	.text
	.global sltu3x
sltu3x:
	sltu3 $15,$11,2
	sltu3 $6,$0,1
	sltu3 $9,$11,3
	sltu3 $0,$4,0
	sltu3 $13,$gp,4
	.text
	.global or
or:
	or $sp,$gp
	or $fp,$3
	or $0,$sp
	or $tp,$0
	or $8,$6
	.text
	.global and
and:
	and $15,$sp
	and $6,$14
	and $4,$2
	and $5,$fp
	and $7,$14
	.text
	.global xor
xor:
	xor $1,$12
	xor $12,$tp
	xor $10,$8
	xor $sp,$11
	xor $12,$8
	.text
	.global nor
nor:
	nor $9,$5
	nor $8,$2
	nor $15,$9
	nor $5,$sp
	nor $sp,$14
	.text
	.global or3
or3:
	or3 $13,$sp,2
	or3 $sp,$tp,3
	or3 $0,$10,4
	or3 $9,$15,3
	or3 $9,$sp,0
	.text
	.global and3
and3:
	and3 $5,$8,1
	and3 $11,$gp,3
	and3 $6,$0,0
	and3 $sp,$sp,0
	and3 $1,$10,3
	.text
	.global xor3
xor3:
	xor3 $0,$0,2
	xor3 $15,$6,0
	xor3 $13,$5,0
	xor3 $15,$7,0
	xor3 $15,$sp,2
	.text
	.global sra
sra:
	sra $4,$1
	sra $fp,$15
	sra $1,$1
	sra $0,$5
	sra $9,$1
	.text
	.global srl
srl:
	srl $2,$11
	srl $15,$7
	srl $1,$7
	srl $3,$13
	srl $14,$1
	.text
	.global sll
sll:
	sll $11,$0
	sll $tp,$fp
	sll $8,$9
	sll $13,$15
	sll $sp,$sp
	.text
	.global srai
srai:
	sra $1,2
	sra $15,3
	sra $sp,3
	sra $6,4
	sra $sp,3
	.text
	.global srli
srli:
	srl $10,0
	srl $9,3
	srl $6,4
	srl $10,2
	srl $8,3
	.text
	.global slli
slli:
	sll $0,0
	sll $4,0
	sll $13,2
	sll $11,2
	sll $6,0
	.text
	.global sll3
sll3:
	sll3 $0,$tp,4
	sll3 $0,$14,0
	sll3 $0,$8,2
	sll3 $0,$3,2
	sll3 $0,$fp,0
	.text
	.global fsft
fsft:
	fsft $gp,$10
	fsft $gp,$9
	fsft $15,$13
	fsft $11,$3
	fsft $5,$3
	.text
	.global bra
bra:
	bra 2
	bra -2
	bra 2
	bra 0
	bra 2
	.text
	.global beqz
beqz:
	beqz $1,-2
	beqz $sp,2
	beqz $4,4
	beqz $4,0
	beqz $9,-2
	.text
	.global bnez
bnez:
	bnez $8,2
	bnez $13,2
	bnez $gp,0
	bnez $6,2
	bnez $8,-4
	.text
	.global beqi
beqi:
	beqi $tp,3,0
	beqi $0,4,-2
	beqi $sp,4,-2
	beqi $13,2,0
	beqi $4,2,-8
	.text
	.global bnei
bnei:
	bnei $8,1,0
	bnei $5,1,2
	bnei $5,0,8
	bnei $9,4,-2
	bnei $0,4,-8
	.text
	.global blti
blti:
	blti $7,3,0
	blti $1,1,0
	blti $8,2,2
	blti $11,2,2
	blti $15,3,-2
	.text
	.global bgei
bgei:
	bgei $4,3,-8
	bgei $7,0,2
	bgei $13,1,0
	bgei $5,2,-2
	bgei $12,4,-8
	.text
	.global beq
beq:
	beq $7,$2,-2
	beq $1,$3,-8
	beq $2,$0,2
	beq $sp,$fp,2
	beq $3,$0,0
	.text
	.global bne
bne:
	bne $6,$3,0
	bne $sp,$3,-8
	bne $8,$0,2
	bne $gp,$sp,8
	bne $sp,$4,2
	.text
	.global bsr12
bsr12:
	bsr 2
	bsr -8
	bsr -16
	bsr -2
	bsr -8
	.text
	.global bsr24
bsr24:
	bsr 4
	bsr -2
	bsr -4
	bsr 0
	bsr 2
	.text
	.global jmp
jmp:
	jmp $2
	jmp $tp
	jmp $5
	jmp $sp
	jmp $fp
	.text
	.global jmp24
jmp24:
	jmp 4
	jmp 2
	jmp 0
	jmp 2
	jmp 4
	.text
	.global jsr
jsr:
	jsr $15
	jsr $13
	jsr $13
	jsr $6
	jsr $6
	.text
	.global ret
ret:
	ret
	.text
	.global repeat
repeat:
	repeat $4,2
	repeat $fp,4
	repeat $0,8
	repeat $6,2
	repeat $4,2
	.text
	.global erepeat
erepeat:
	erepeat 2
	erepeat 0
	erepeat 2
	erepeat -2
	erepeat 0
	.text
	.global stc
stc:
	stc $13,$mb1
	stc $tp,$ccfg
	stc $11,$dbg
	stc $10,$ccfg
	stc $9,$epc
	.text
	.global ldc
ldc:
	ldc $tp,$lo
	ldc $8,$npc
	ldc $9,$mb0
	ldc $15,$sar
	ldc $9,$ccfg
	.text
	.global di
di:
	di
	.text
	.global ei
ei:
	ei
	.text
	.global reti
reti:
	reti
	.text
	.global halt
halt:
	halt
	.text
	.global swi
swi:
	swi 2
	swi 0
	swi 2
	swi 3
	swi 1
	.text
	.global break
break:
	break
	.text
	.global sycnm
syncm:
	syncm
	.text
	.global stcb
stcb:
	stcb $5,4
	stcb $5,1
	stcb $gp,0
	stcb $15,4
	stcb $11,2
	.text
	.global ldcb
ldcb:
	ldcb $2,3
	ldcb $2,4
	ldcb $9,1
	ldcb $10,4
	ldcb $1,4
	.text
	.global bsetm
bsetm:
	bsetm ($10),0
	bsetm ($sp),0
	bsetm ($1),2
	bsetm ($sp),4
	bsetm ($8),4
	.text
	.global bclrm
bclrm:
	bclrm ($5),0
	bclrm ($5),2
	bclrm ($8),0
	bclrm ($9),2
	bclrm ($5),3
	.text
	.global bnotm
bnotm:
	bnotm ($14),4
	bnotm ($11),4
	bnotm ($10),0
	bnotm ($tp),4
	bnotm ($fp),0
	.text
	.global btstm
btstm:
	btstm $0,($14),0
	btstm $0,($14),1
	btstm $0,($11),0
	btstm $0,($14),3
	btstm $0,($fp),2
	.text
	.global tas
tas:
	tas $7,($tp)
	tas $7,($12)
	tas $3,($fp)
	tas $2,($5)
	tas $6,($10)
	.text
	.global cache
cache:
	cache 1,($13)
	cache 3,($12)
	cache 3,($9)
	cache 4,($2)
	cache 4,($7)
	.text
	.global mul
mul:
	mul $8,$14
	mul $2,$9
	mul $14,$15
	mul $9,$7
	mul $7,$11
	.text
	.global mulu
mulu:
	mulu $2,$5
	mulu $6,$gp
	mulu $gp,$sp
	mulu $11,$14
	mulu $3,$9
	.text
	.global mulr
mulr:
	mulr $12,$6
	mulr $13,$8
	mulr $7,$10
	mulr $gp,$1
	mulr $0,$15
	.text
	.global mulru
mulru:
	mulru $4,$2
	mulru $14,$1
	mulru $15,$4
	mulru $10,$6
	mulru $0,$gp
	.text
	.global madd
madd:
	madd $4,$11
	madd $15,$14
	madd $14,$sp
	madd $4,$tp
	madd $1,$gp
	.text
	.global maddu
maddu:
	maddu $0,$1
	maddu $7,$6
	maddu $9,$5
	maddu $gp,$15
	maddu $7,$13
	.text
	.global maddr
maddr:
	maddr $6,$fp
	maddr $9,$14
	maddr $8,$gp
	maddr $3,$2
	maddr $1,$11
	.text
	.global maddru
maddru:
	maddru $10,$3
	maddru $15,$12
	maddru $8,$fp
	maddru $14,$3
	maddru $fp,$15
	.text
	.global div
div:
	div $9,$3
	div $4,$14
	div $2,$12
	div $fp,$tp
	div $tp,$6
	.text
	.global divu
divu:
	divu $9,$5
	divu $8,$13
	divu $0,$14
	divu $9,$5
	divu $0,$5
	.text
	.global dret
dret:
	dret
	.text
	.global dbreak
dbreak:
	dbreak
	.text
	.global ldz
ldz:
	ldz $gp,$4
	ldz $10,$11
	ldz $9,$9
	ldz $15,$tp
	ldz $gp,$3
	.text
	.global abs
abs:
	abs $sp,$9
	abs $5,$4
	abs $tp,$13
	abs $0,$3
	abs $3,$14
	.text
	.global ave
ave:
	ave $11,$10
	ave $fp,$10
	ave $14,$2
	ave $10,$12
	ave $15,$8
	.text
	.global min
min:
	min $8,$3
	min $7,$0
	min $2,$2
	min $5,$6
	min $11,$5
	.text
	.global max
max:
	max $11,$sp
	max $gp,$0
	max $12,$sp
	max $gp,$2
	max $14,$sp
	.text
	.global minu
minu:
	minu $11,$8
	minu $7,$5
	minu $fp,$14
	minu $11,$4
	minu $2,$sp
	.text
	.global maxu
maxu:
	maxu $3,$3
	maxu $13,$0
	maxu $4,$fp
	maxu $gp,$2
	maxu $12,$fp
	.text
	.global clip
clip:
	clip $10,1
	clip $15,4
	clip $4,3
	clip $15,3
	clip $1,0
	.text
	.global clipu
clipu:
	clipu $10,4
	clipu $13,1
	clipu $5,4
	clipu $14,0
	clipu $5,1
	.text
	.global sadd
sadd:
	sadd $5,$0
	sadd $15,$3
	sadd $0,$10
	sadd $sp,$12
	sadd $4,$2
	.text
	.global ssub
ssub:
	ssub $1,$10
	ssub $4,$7
	ssub $fp,$3
	ssub $7,$gp
	ssub $13,$4
	.text
	.global saddu
saddu:
	saddu $9,$14
	saddu $0,$10
	saddu $7,$12
	saddu $5,$15
	saddu $13,$3
	.text
	.global ssubu
ssubu:
	ssubu $15,$gp
	ssubu $0,$15
	ssubu $3,$10
	ssubu $sp,$13
	ssubu $2,$9
	.text
	.global swcp
swcp:
	swcp $c3,($13)
	swcp $c15,($13)
	swcp $c13,($0)
	swcp $c12,($12)
	swcp $c9,($gp)
	.text
	.global lwcp
lwcp:
	lwcp $c7,($3)
	lwcp $c6,($3)
	lwcp $c0,($2)
	lwcp $c8,($fp)
	lwcp $c11,($13)
	.text
	.global smcp
smcp:
	smcp $c14,($9)
	smcp $c2,($fp)
	smcp $c14,($15)
	smcp $c10,($8)
	smcp $c2,($8)
	.text
	.global lmcp
lmcp:
	lmcp $c11,($1)
	lmcp $c8,($8)
	lmcp $c11,($13)
	lmcp $c8,($0)
	lmcp $c8,($14)
	.text
	.global swcpi
swcpi:
	swcpi $c7,($0+)
	swcpi $c6,($gp+)
	swcpi $c12,($8+)
	swcpi $c14,($15+)
	swcpi $c6,($0+)
	.text
	.global lwcpi
lwcpi:
	lwcpi $c8,($2+)
	lwcpi $c9,($0+)
	lwcpi $c3,($14+)
	lwcpi $c13,($5+)
	lwcpi $c11,($gp+)
	.text
	.global smcpi
smcpi:
	smcpi $c8,($2+)
	smcpi $c11,($9+)
	smcpi $c4,($3+)
	smcpi $c14,($2+)
	smcpi $c9,($3+)
	.text
	.global lmcpi
lmcpi:
	lmcpi $c6,($14+)
	lmcpi $c9,($5+)
	lmcpi $c10,($6+)
	lmcpi $c1,($6+)
	lmcpi $c2,($8+)
	.text
	.global swcp16
swcp16:
	swcp $c0,-1($2)
	swcp $c5,1($10)
	swcp $c8,2($12)
	swcp $c14,-1($1)
	swcp $c12,2($3)
	.text
	.global lwcp16
lwcp16:
	lwcp $c8,-1($5)
	lwcp $c12,1($15)
	lwcp $c1,2($0)
	lwcp $c4,1($13)
	lwcp $c6,2($11)
	.text
	.global smcp16
smcp16:
	smcp $c9,-1($10)
	smcp $c14,1($gp)
	smcp $c3,2($sp)
	smcp $c15,-2($8)
	smcp $c13,1($13)
	.text
	.global lmcp16
lmcp16:
	lmcp $c0,1($15)
	lmcp $c15,1($fp)
	lmcp $c2,-1($8)
	lmcp $c14,1($fp)
	lmcp $c1,-1($10)
	.text
	.global sbcpa
sbcpa:
	sbcpa $c14,($sp+),2
	sbcpa $c2,($4+),-2
	sbcpa $c8,($1+),0
	sbcpa $c11,($3+),0
	sbcpa $c9,($14+),-2
	.text
	.global lbcpa
lbcpa:
	lbcpa $c7,($2+),-2
	lbcpa $c12,($sp+),2
	lbcpa $c5,($4+),-2
	lbcpa $c7,($4+),-2
	lbcpa $c8,($15+),0
	.text
	.global shcpa
shcpa:
	shcpa $c0,($14+),0
	shcpa $c12,($sp+),16
	shcpa $c1,($4+),4
	shcpa $c5,($4+),-32
	shcpa $c1,($15+),0
	.text
	.global lhcpa
lhcpa:
	lhcpa $c4,($4+),0
	lhcpa $c6,($5+),48
	lhcpa $c3,($6+),-52
	lhcpa $c8,($6+),-24
	lhcpa $c0,($9+),0
	.text
	.global swcpa
swcpa:
	swcpa $c1,($9+),16
	swcpa $c7,($sp+),32
	swcpa $c3,($12+),48
	swcpa $c10,($9+),8
	swcpa $c14,($8+),4
	.text
	.global lwcpa
lwcpa:
	lwcpa $c6,($gp+),-8
	lwcpa $c4,($7+),4
	lwcpa $c11,($gp+),-16
	lwcpa $c10,($sp+),-32
	lwcpa $c2,($2+),8
	.text
	.global smcpa
smcpa:
	smcpa $c13,($15+),-8
	smcpa $c6,($7+),-8
	smcpa $c5,($3+),16
	smcpa $c13,($15+),16
	smcpa $c3,($12+),48
	.text
	.global lmcpa
lmcpa:
	lmcpa $c9,($4+),0
	lmcpa $c3,($sp+),-16
	lmcpa $c15,($13+),8
	lmcpa $c8,($8+),-8
	lmcpa $c10,($9+),0
	.text
	.global sbcpm0
sbcpm0:
	sbcpm0 $c10,($13+),8
	sbcpm0 $c13,($5+),-8
	sbcpm0 $c4,($5+),-8
	sbcpm0 $c10,($tp+),16
	sbcpm0 $c4,($5+),-24
	.text
	.global lbcpm0
lbcpm0:
	lbcpm0 $c0,($4+),0
	lbcpm0 $c9,($7+),-8
	lbcpm0 $c12,($fp+),24
	lbcpm0 $c8,($12+),16
	lbcpm0 $c7,($fp+),16
	.text
	.global shcpm0
shcpm0:
	shcpm0 $c2,($13+),2
	shcpm0 $c7,($15+),-2
	shcpm0 $c8,($2+),2
	shcpm0 $c13,($5+),0
	shcpm0 $c3,($14+),8
	.text
	.global lhcpm0
lhcpm0:
	lhcpm0 $c7,($4+),8
	lhcpm0 $c3,($3+),-2
	lhcpm0 $c3,($1+),0
	lhcpm0 $c2,($gp+),0
	lhcpm0 $c12,($6+),2
	.text
	.global swcpm0
swcpm0:
	swcpm0 $c8,($fp+),32
	swcpm0 $c9,($sp+),0
	swcpm0 $c9,($2+),-16
	swcpm0 $c0,($14+),48
	swcpm0 $c15,($1+),8
	.text
	.global lwcpm0
lwcpm0:
	lwcpm0 $c14,($10+),-4
	lwcpm0 $c11,($sp+),-4
	lwcpm0 $c5,($7+),-8
	lwcpm0 $c2,($12+),32
	lwcpm0 $c2,($gp+),16
	.text
	.global smcpm0
smcpm0:
	smcpm0 $c1,($12+),8
	smcpm0 $c8,($4+),-16
	smcpm0 $c10,($11+),0
	smcpm0 $c1,($3+),-16
	smcpm0 $c11,($sp+),-8
	.text
	.global lmcpm0
lmcpm0:
	lmcpm0 $c14,($10+),0
	lmcpm0 $c6,($15+),-16
	lmcpm0 $c13,($1+),8
	lmcpm0 $c10,($tp+),-24
	lmcpm0 $c7,($14+),-24
	.text
	.global sbcpm1
sbcpm1:
	sbcpm1 $c9,($fp+),0
	sbcpm1 $c7,($12+),-24
	sbcpm1 $c15,($5+),-24
	sbcpm1 $c5,($tp+),16
	sbcpm1 $c6,($1+),-128
	.text
	.global lbcpm1
lbcpm1:
	lbcpm1 $c6,($gp+),2
	lbcpm1 $c7,($tp+),-2
	lbcpm1 $c4,($13+),1
	lbcpm1 $c12,($2+),-2
	lbcpm1 $c11,($7+),1
	.text
	.global shcpm1
shcpm1:
	shcpm1 $c4,($fp+),24
	shcpm1 $c11,($6+),-16
	shcpm1 $c7,($8+),8
	shcpm1 $c5,($12+),16
	shcpm1 $c0,($8+),-32
	.text
	.global lhcpm1
lhcpm1:
	lhcpm1 $c11,($0+),0
	lhcpm1 $c7,($tp+),-2
	lhcpm1 $c10,($8+),8
	lhcpm1 $c3,($tp+),0
	lhcpm1 $c9,($6+),2
	.text
	.global swcpm1
swcpm1:
	swcpm1 $c9,($8+),24
	swcpm1 $c9,($14+),0
	swcpm1 $c9,($fp+),16
	swcpm1 $c14,($1+),0
	swcpm1 $c2,($sp+),8
	.text
	.global lwcpm1
lwcpm1:
	lwcpm1 $c8,($fp+),0
	lwcpm1 $c3,($14+),-16
	lwcpm1 $c7,($6+),-8
	lwcpm1 $c14,($fp+),-24
	lwcpm1 $c3,($fp+),24
	.text
	.global smcpm1
smcpm1:
	smcpm1 $c10,($4+),0
	smcpm1 $c6,($sp+),-16
	smcpm1 $c13,($7+),-24
	smcpm1 $c3,($gp+),-8
	smcpm1 $c0,($2+),8
	.text
	.global lmcpm1
lmcpm1:
	lmcpm1 $c12,($1+),0
	lmcpm1 $c0,($6+),8
	lmcpm1 $c6,($2+),-8
	lmcpm1 $c12,($gp+),-16
	lmcpm1 $c14,($15+),48
/*	
	.text
	.global cmov1
cmov1:
	cmov $c11,$10
	cmov $c14,$3
	cmov $c3,$15
	cmov $c6,$5
	cmov $c6,$10
	.text
	.global cmov2
cmov2:
	cmov $11,$c2
	cmov $10,$c2
	cmov $tp,$c10
	cmov $12,$c9
	cmov $15,$c3
	.text
	.global cmovc1
cmovc1:
	cmovc $ccr9,$sp
	cmovc $ccr12,$fp
	cmovc $ccr1,$4
	cmovc $ccr11,$sp
	cmovc $ccr14,$7
	.text
	.global cmovc2
cmovc2:
	cmovc $fp,$ccr6
	cmovc $fp,$ccr6
	cmovc $7,$ccr8
	cmovc $sp,$ccr12
	cmovc $sp,$ccr5
	.text
	.global cmovh1
cmovh1:
	cmovh $c8,$1
	cmovh $c12,$sp
	cmovh $c11,$5
	cmovh $c4,$4
	cmovh $c3,$gp
	.text
	.global cmovh2
cmovh2:
	cmovh $4,$c7
	cmovh $gp,$c8
	cmovh $6,$c10
	cmovh $2,$c8
	cmovh $10,$c4
*/	
	.text
	.global bcpeq
bcpeq:
	bcpeq 4,0
	bcpeq 0,-2
	bcpeq 4,-2
	bcpeq 1,2
	bcpeq 2,2
	.text
	.global bcpne
bcpne:
	bcpne 2,0
	bcpne 4,0
	bcpne 1,0
	bcpne 4,0
	bcpne 1,2
	.text
	.global bcpat
bcpat:
	bcpat 1,-2
	bcpat 0,2
	bcpat 0,-2
	bcpat 2,0
	bcpat 1,-2
	.text
	.global bcpaf
bcpaf:
	bcpaf 4,0
	bcpaf 3,0
	bcpaf 4,0
	bcpaf 1,2
	bcpaf 4,2
	.text
	.global synccp
synccp:
	synccp
	.text
	.global jsrv
jsrv:
	jsrv $11
	jsrv $5
	jsrv $10
	jsrv $12
	jsrv $10
	.text
	.global bsrv
bsrv:
	bsrv -2
	bsrv -2
	bsrv -2
	bsrv 2
	bsrv 0
	.text
	.global case106341
case106341:
	stc $10,7
	ldc $0, (4 + 4)
case106821:
	/* Actual 16 bit form */
        sb      $0,($0)
        sh      $0,($0)
        sw      $0,($0)
        lb      $0,($0)
        lh      $0,($0)
        lw      $0,($0)
        lbu     $0,($0)
        lhu     $0,($0)
	/* Should use 16 bit form */
        sb      $0,0($0)
        sb      $0,%lo(0)($0)
        sb      $0,%hi(0)($0)
        sb      $0,%uhi(0)($0)
        sb      $0,%sdaoff(0)($0)
        sb      $0,%tpoff(0)($0)
        sh      $0,0($0)
        sh      $0,%lo(0)($0)
        sh      $0,%hi(0)($0)
        sh      $0,%uhi(0)($0)
        sh      $0,%sdaoff(0)($0)
        sh      $0,%tpoff(0)($0)
        sw      $0,0($0)
        sw      $0,%lo(0)($0)
        sw      $0,%hi(0)($0)
        sw      $0,%uhi(0)($0)
        sw      $0,%sdaoff(0)($0)
        sw      $0,%tpoff(0)($0)
        lb      $0,0($0)
        lb      $0,%lo(0)($0)
        lb      $0,%hi(0)($0)
        lb      $0,%uhi(0)($0)
        lb      $0,%sdaoff(0)($0)
        lb      $0,%tpoff(0)($0)
        lh      $0,0($0)
        lh      $0,%lo(0)($0)
        lh      $0,%hi(0)($0)
        lh      $0,%uhi(0)($0)
        lh      $0,%sdaoff(0)($0)
        lh      $0,%tpoff(0)($0)
        lw      $0,0($0)
        lw      $0,%lo(0)($0)
        lw      $0,%hi(0)($0)
        lw      $0,%uhi(0)($0)
        lw      $0,%sdaoff(0)($0)
        lw      $0,%tpoff(0)($0)
        lbu     $0,0($0)
        lbu     $0,%lo(0)($0)
        lbu     $0,%hi(0)($0)
        lbu     $0,%uhi(0)($0)
        lbu     $0,%sdaoff(0)($0)
        lbu     $0,%tpoff(0)($0)
        lhu     $0,0($0)
        lhu     $0,%lo(0)($0)
        lhu     $0,%hi(0)($0)
        lhu     $0,%uhi(0)($0)
        lhu     $0,%sdaoff(0)($0)
        lhu     $0,%tpoff(0)($0)
	/* Should use 32 bit form */
        sb      $0,1($0)
        sb      $0,%lo(1)($0)
        sb      $0,%hi(1)($0)
        sb      $0,%uhi(1)($0)
        sb      $0,%sdaoff(1)($0)
        sb      $0,%tpoff(1)($0)
        sh      $0,1($0)
        sh      $0,%lo(1)($0)
        sh      $0,%hi(1)($0)
        sh      $0,%uhi(1)($0)
        sh      $0,%sdaoff(1)($0)
        sh      $0,%tpoff(1)($0)
        sw      $0,1($0)
        sw      $0,%lo(1)($0)
        sw      $0,%hi(1)($0)
        sw      $0,%uhi(1)($0)
        sw      $0,%sdaoff(1)($0)
        sw      $0,%tpoff(1)($0)
        lb      $0,1($0)
        lb      $0,%lo(1)($0)
        lb      $0,%hi(1)($0)
        lb      $0,%uhi(1)($0)
        lb      $0,%sdaoff(1)($0)
        lb      $0,%tpoff(1)($0)
        lh      $0,1($0)
        lh      $0,%lo(1)($0)
        lh      $0,%hi(1)($0)
        lh      $0,%uhi(1)($0)
        lh      $0,%sdaoff(1)($0)
        lh      $0,%tpoff(1)($0)
        lw      $0,1($0)
        lw      $0,%lo(1)($0)
        lw      $0,%hi(1)($0)
        lw      $0,%uhi(1)($0)
        lw      $0,%sdaoff(1)($0)
        lw      $0,%tpoff(1)($0)
        lbu     $0,1($0)
        lbu     $0,%lo(1)($0)
        lbu     $0,%hi(1)($0)
        lbu     $0,%uhi(1)($0)
        lbu     $0,%sdaoff(1)($0)
        lbu     $0,%tpoff(1)($0)
        lhu     $0,1($0)
        lhu     $0,%lo(1)($0)
        lhu     $0,%hi(1)($0)
        lhu     $0,%uhi(1)($0)
        lhu     $0,%sdaoff(1)($0)
        lhu     $0,%tpoff(1)($0)
	/* Should use 32 bit form */
	sb      $0,case106821($0)
        sb      $0,%lo(case106821)($0)
        sb      $0,%hi(case106821)($0)
        sb      $0,%uhi(case106821)($0)
	sh      $0,case106821($0)
        sh      $0,%lo(case106821)($0)
        sh      $0,%hi(case106821)($0)
        sh      $0,%uhi(case106821)($0)
	sw      $0,case106821($0)
        sw      $0,%lo(case106821)($0)
        sw      $0,%hi(case106821)($0)
        sw      $0,%uhi(case106821)($0)
	lb      $0,case106821($0)
        lb      $0,%lo(case106821)($0)
        lb      $0,%hi(case106821)($0)
        lb      $0,%uhi(case106821)($0)
	lh      $0,case106821($0)
        lh      $0,%lo(case106821)($0)
        lh      $0,%hi(case106821)($0)
        lh      $0,%uhi(case106821)($0)
	lw      $0,case106821($0)
        lw      $0,%lo(case106821)($0)
        lw      $0,%hi(case106821)($0)
        lw      $0,%uhi(case106821)($0)
	lbu     $0,case106821($0)
        lbu     $0,%lo(case106821)($0)
        lbu     $0,%hi(case106821)($0)
        lbu     $0,%uhi(case106821)($0)
	lhu     $0,case106821($0)
        lhu     $0,%lo(case106821)($0)
        lhu     $0,%hi(case106821)($0)
        lhu     $0,%uhi(case106821)($0)
