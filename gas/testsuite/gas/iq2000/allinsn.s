 .data
foodata: .word 42
 .text
footext:
	.text
	.global add
add:
	add %0,%0,%0
	.text
	.global addi
addi:
	addi %0,%0,-4
	.text
	.global addiu
addiu:
	addiu %0,%0,4
	.text
	.global addu
addu:
	addu %0,%0,%0
	.text
	.global ado16
ado16:
	ado16 %0,%0,%0
	.text
	.global and
and:
	and %0,%0,%0
	.text
	.global andi
andi:
	andi %0,%0,0xdead
	.text
	.global andoi
andoi:
	andoi %0,%0,0
	.text
	.global andoui
andoui:
	andoui %0,%0,0
	.text
	.global mrgb
mrgb:
	mrgb %0,%0,%0,0
	.text
	.global nor
nor:
	nor %0,%0,%0
	.text
	.global or
or:
	or %0,%0,%0
	.text
	.global ori
ori:
	ori %0,%0,-1
	.text
	.global orui
orui:
	orui %0,%0,0
	.text
	.global ram
ram:
	ram %0,%0,0,0,0
	.text
	.global sll
sll:
	sll %0,%0,0
	.text
	.global sllv
sllv:
	sllv %0,%0,%0
	.text
	.global slmv
slmv:
	slmv %0,%0,%0,0
	.text
	.global slt
slt:
	slt %0,%0,%0
	.text
	.global slti
slti:
	slti %0,%0,0
	.text
	.global sltiu
sltiu:
	sltiu %0,%0,0
	.text
	.global sltu
sltu:
	sltu %0,%0,%0
	.text
	.global sra
sra:
	sra %0,%0,0
	.text
	.global srav
srav:
	srav %0,%0,%0
	.text
	.global srl
srl:
	srl %0,%0,0
	.text
	.global srlv
srlv:
	srlv %0,%0,%0
	.text
	.global srmv
srmv:
	srmv %0,%0,%0,0
	.text
	.global sub
sub:
	sub %0,%0,%0
	.text
	.global subu
subu:
	subu %0,%0,%0
	.text
	.global xor
xor:
	xor %0,%0,%0
	.text
	.global xori
xori:
	xori %0,%0,0
	.text
	.global bbi
bbi:
	bbi %0(0),footext
	.text
	.global bbin
bbin:
	bbin %0(0),footext
	.text
	.global bbv
bbv:
	bbv %0,%0,footext
	.text
	.global bbvn
bbvn:
	bbvn %0,%0,footext
	.text
	.global beq
beq:
	beq %0,%0,footext
	.text
	.global beql
beql:
	beql %0,%0,footext
	.text
	.global bgez
bgez:
	bgez %0,footext
	.text
	.global bgezal
bgezal:
	bgezal %0,footext
	.text
	.global bgezall
bgezall:
	bgezall %0,footext
	.text
	.global bgezl
bgezl:
	bgezl %0,footext
	.text
	.global bgtz
bgtz:
	bgtz %0,footext
	.text
	.global bgtzl
bgtzl:
	bgtzl %0,footext
	.text
	.global blez
blez:
	blez %0,footext
	.text
	.global blezl
blezl:
	blezl %0,footext
	.text
	.global bltz
bltz:
	bltz %0,footext
	.text
	.global bltzl
bltzl:
	bltzl %0,footext
	.text
	.global bltzal
bltzal:
	bltzal %0,footext
	.text
	.global bltzall
bltzall:
	bltzall %0,footext
	.text
	.global bmb
bmb:
	bmb %0,%0,footext
	.text
	.global bmb0
bmb0:
	bmb0 %0,%0,footext
	.text
	.global bmb1
bmb1:
	bmb1 %0,%0,footext
	.text
	.global bmb2
bmb2:
	bmb2 %0,%0,footext
	.text
	.global bmb3
bmb3:
	bmb3 %0,%0,footext
	.text
	.global bne
bne:
	bne %0,%0,footext
	.text
	.global bnel
bnel:
	bnel %0,%0,footext
	.text
	.global bctxt
bctxt:
	bctxt %0,footext
	.text
	.global bc0f
bc0f:
	bc0f footext
	.text
	.global bc0fl
bc0fl:
	bc0fl footext
	.text
	.global bc3f
bc3f:
	bc3f footext
	.text
	.global bc3fl
bc3fl:
	bc3fl footext
	.text
	.global bc0t
bc0t:
	bc0t footext
	.text
	.global bc0tl
bc0tl:
	bc0tl footext
	.text
	.global bc3t
bc3t:
	bc3t footext
	.text
	.global bc3tl
bc3tl:
	bc3tl footext
	.text
	.global break
break:
	break
	.text
	.global cfc0
cfc0:
	cfc0 %0,%0
	.text
	.global cfc1
cfc1:
	cfc1 %0,%0
	.text
	.global cfc2
cfc2:
	cfc2 %0,%0
	.text
	.global cfc3
cfc3:
	cfc3 %0,%0
	.text
	.global chkhdr
chkhdr:
	chkhdr %0,%0
	.text
	.global ctc0
ctc0:
	ctc0 %0,%0
	.text
	.global ctc1
ctc1:
	ctc1 %0,%0
	.text
	.global ctc2
ctc2:
	ctc2 %0,%0
	.text
	.global ctc3
ctc3:
	ctc3 %0,%0
	.text
	.global jcr
jcr:
	jcr %0
	.text
	.global luc32
	nop
luc32:
	# insert a nop here to pacify the assembler (luc32 may not follow jcr).
	luc32 %0,%0
	.text
	.global luc32l
luc32l:
	luc32l %0,%0
	.text
	.global luc64
luc64:
	luc64 %0,%0
	.text
	.global luc64l
luc64l:
	luc64l %0,%0
	.text
	.global luk
luk:
	luk %0,%0
	.text
	.global lulck
lulck:
	lulck %0
	.text
	.global lum32
lum32:
	lum32 %0,%0
	.text
	.global lum32l
lum32l:
	lum32l %0,%0
	.text
	.global lum64
lum64:
	lum64 %0,%0
	.text
	.global lum64l
lum64l:
	lum64l %0,%0
	.text
	.global lur
lur:
	lur %0,%0
	.text
	.global lurl
lurl:
	lurl %0,%0
	.text
	.global luulck
luulck:
	luulck %0
	.text
	.global mfc0
mfc0:
	mfc0 %0,%0
	.text
	.global mfc1
mfc1:
	mfc1 %0,%0
	.text
	.global mfc2
mfc2:
	mfc2 %0,%0
	.text
	.global mfc3
mfc3:
	mfc3 %0,%0
	.text
	.global mtc0
mtc0:
	mtc0 %0,%0
	.text
	.global mtc1
mtc1:
	mtc1 %0,%0
	.text
	.global mtc2
mtc2:
	mtc2 %0,%0
	.text
	.global mtc3
mtc3:
	mtc3 %0,%0
	.text
	.global rb
rb:
	rb %0,%0
	.text
	.global rbr1
rbr1:
	rbr1 %0,0,0
	.text
	.global rbr30
rbr30:
	rbr30 %0,0,0
	.text
	.global rfe
rfe:
	rfe
	.text
	.global rx
rx:
	rx %0,%0
	.text
	.global rxr1
rxr1:
	rxr1 %0,0,0
	.text
	.global rxr30
rxr30:
	rxr30 %0,0,0
	.text
	.global sleep
sleep:
	sleep
	.text
	.global srrd
srrd:
	srrd %0
	.text
	.global srrdl
srrdl:
	srrdl %0
	.text
	.global srulck
srulck:
	srulck %0
	.text
	.global srwr
srwr:
	srwr %0,%0
	.text
	.global srwru
srwru:
	srwru %0,%0
	.text
	.global syscall
syscall:
	syscall
	.text
	.global trapqfl
trapqfl:
	trapqfl
	.text
	.global trapqne
trapqne:
	trapqne
	.text
	.global wb
wb:
	wb %0,%0
	.text
	.global wbu
wbu:
	wbu %0,%0
	.text
	.global wbr1
wbr1:
	wbr1 %3,0,0
	.text
	.global wbr1u
wbr1u:
	wbr1u %0,0,0
	.text
	.global wbr30
wbr30:
	wbr30 %0,0,0
	.text
	.global wbr30u
wbr30u:
	wbr30u %0,0,0
	.text
	.global wx
wx:
	wx %0,%0
	.text
	.global wxu
wxu:
	wxu %0,%0
	.text
	.global wxr1
wxr1:
	wxr1 %0,0,0
	.text
	.global wxr1u
wxr1u:
	wxr1u %0,0,0
	.text
	.global wxr30
wxr30:
	wxr30 %0,0,0
	.text
	.global wxr30u
wxr30u:
	wxr30u %0,0,0
	.text
	.global j
j:
	j footext
	.text
	.global jal
jal:
	jal footext
	.text
	.global jalr
jalr:
	jalr %0,%0
	.text
	.global jr
jr:
	jr %0
	.text
	.global lb
lb:
	lb %0,0x1024(%0)
	.text
	.global lbu
lbu:
	lbu %0,0x1024(%0)
	.text
	.global ldw
ldw:
	ldw %0,0x1024(%0)
	.text
	.global lh
lh:
	lh %0,0x1024(%0)
	.text
	.global lhu
lhu:
	lhu %0,0x1024(%0)
	.text
	.global lui
lui:
	lui %0,-1
	.text
	.global lw
lw:
	lw %0,0x1024(%0)
	.text
	.global sb
sb:
	sb %0,0x1024(%0)
	.text
	.global sdw
sdw:
	sdw %0,0x1024(%0)
	.text
	.global sh
sh:
	sh %0,0x1024(%0)
	.text
	.global sw
sw:
	sw %0,0x1024(%0)
	.text
	.global traprel
traprel:
	traprel %0
	.text
	.global pkrl
pkrl:
	pkrl %0,%1
	.text
	.global pkrlr1
pkrlr1:
	pkrlr1 %0,0,0
	.text
	.global pkrlr30
pkrlr30:
	pkrlr30 %0,0,0
