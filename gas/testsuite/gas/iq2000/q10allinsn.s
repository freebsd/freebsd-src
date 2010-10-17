	.globl foodata	
	.data
	.align  2
foodata: 
	.word 42		
	.text
	.global add
add:
	add %0,%29,%30
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
	addu %0,%29,%30
	.text
	.global ado16
ado16:
	ado16 %0,%29,%30
	.text
	.global and
and:
	and %0,%29,%30
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
	mrgb %0,%29,%0,0
	.text
	.global nor
nor:
	nor %0,%29,%30
	.text
	.global or
or:
	or %0,%29,%30
	or %1,%29,%30
	.text
	.global ori
ori:
	ori %0,%0,-1
	.text
	.global orui
orui:
	orui %0,%1,0
	.text
	.global ram
ram:
	ram %0,%0,0,0,0
	.text
	.global sll
sll:
	sll %0,%0,0
	sll %1,%2,0
	.text
	.global sllv
sllv:
	sllv %0,%29,%30
	.text
	.global slmv
slmv:
	slmv %0,%0,%0,0
	.text
	.global slt
slt:
	slt %0,%29,%30
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
	sltu %0,%29,%30
	.text
	.global sra
sra:
	sra %0,%0,0
	.text
	.global srav
srav:
	srav %0,%29,%30
	.text
	.global srl
srl:
	srl %0,%0,0
	.text
	.global srlv
srlv:
	srlv %0,%29,%30
	.text
	.global srmv
srmv:
	srmv %0,%0,%0,0
	.text
	.global sub
sub:
	sub %0,%29,%30
	.text
	.global subu
subu:
	subu %0,%29,%30
	.text
	.global xor
xor:
	xor %0,%0,%0
	.global xori
xori:
	xori %0,%0,0
footext:
	.text
	.global bbi
bbi:
	bbi %0(0),footext
	.text
	.global bbil
bbil:	
	bbil %0(0),footext
	.text
	.global bbinl
bbinl:	
	bbinl %0(0),footext
	.text
	.global bbin
bbin:
	bbin %0(0),footext
	.text
	.global bbv
bbv:
	bbv %0,%0,footext
	.text
	.global bbvl
bbvl:	
	bbvl %0,%0,footext
	.text
	.global bbvn
bbvn:
	bbvn %0,%0,footext
	.text
	.global bbvnl
bbvnl:
	bbvnl %0,%0,footext
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
	.global bgtzal
bgtzal:
	bgtzal %0,footext
	.text
	.global bgtzall
bgtzall:
	bgtzall %0,footext
	.text
	.global bgtzl
bgtzl:
	bgtzl %0,footext
	.text
	.global blez
blez:
	blez %0,footext
	.text
	.global blezal
blezal:
	blezal %0,footext
	.text
	.global blezall
blezall:
	blezall %0,footext
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
	.global bmbl
bmbl:
	bmbl %0,%0,footext
	.text
	.global bne
bne:
	bne %0,%0,footext
	.text
	.global bnel
bnel:
	bnel %0,%0,footext
	.text
	.global break
break:
	break
	.text
	.global bri
bri:
	bri %0,footext
	.text
	.global brv
brv:
	brv %0,footext
	.text
	.global chkhdr
chkhdr:
	chkhdr %0,%0
	.text
	.global j
j:
	j bartext
	.text
	.global jal
jal:
	jal %0,bartext
bartext:
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
	lui %29,%hi(foodata)
	ori %29,%29,%lo(foodata)
	.text
	.global la
la:
	la %11,foodata
	.global lw
lw:
	lw %0,0x1024(%0)
	.text
	.global sb
sb:
	sb %0,0x1024(%0)
	.text
	.global sh
sh:
	sh %0,0x1024(%0)
	.text
	.global sw
sw:
	sw %0,0x1024(%0)
	.text
	.global swrd
swrd:	
	swrd %29,%30
	.text
	.global swrdl
swrdl:		
	swrdl %29,%30
	.text
	.global swwr
swwr:		
	swwr %0,%29,%30
	.text
	.global swwru
swwru:		
	swwru %0,%29,%30
	.text
	.global rba
rba:		
	rba %0,%29,%30
	.text
	.global rbal
rbal:	
	rbal %0,%29,%30
	.text
	.global rbar
rbar:	
	rbar %0,%29,%30
	.text
	.global dwrd
dwrd:	
	dwrd %28,%30
	.text
	.global dwrdl
dwrdl:	
	dwrdl %28,%30
	.text
	.global wba
wba:						
	wba %0,%29,%30
	.text
	.global wbau
wbau:		
	wbau %0,%29,%30
	.text
	.global wbac
wbac:		
	wbac %0,%29,%30
	.text
	.global crc32
crc32:
	crc32 %0,%29,%30
	.text
	.global crc32b
crc32b:
	crc32b %0,%29,%30
	.text
	.global cfc
cfc:	
	cfc %29,%30
	.text
	.global lock
lock:	
	lock %29,%28
	.text
	.global ctc
ctc:					
	ctc %29,%30
	.text
	.global unlk
unlk:		
	unlk %29,%30
	.text
	.global mcid
mcid:		
	mcid %0,%29
	.text
	.global dba
dba:
	dba %30
	.text
	.global dbd
dbd:	
	dbd %0,%30
	.text
	.global dpwt
dpwt:			
	dpwt %0,%30
	.text
	.global avail
avail:
	avail %31
	.text
	.global free
free:	
	free %0,%30
	.text
	.global tstod
tstod:	
	tstod %0,%30
	.global yield
yield:	
	yield
	.text
	.global pkrla
pkrla:	
	pkrla %0,%29,%30
	.text
	.global pkrlac
pkrlac:	
	pkrlac %0,%29,%30
	.text
	.global pkrlau
pkrlau:	
	pkrlau %0,%29,%30
	.text
	.global pkrlah
pkrlah:	
	pkrlah %0,%29,%30
	.text
	.global cmphdr
cmphdr:						
	cmphdr %31
	.text
	.global cam36
cam36:	
	cam36 %29,%30,1,1
	.text
	.global cam72
cam72:	
	cam72 %0,%30,2,2
	.text
	.global cam144
cam144:	
	cam144 %0,%29,3,3
	.text
	.global cam288
cam288:	
	cam144 %0,%29,4,4
	.text
	.global cm32and
cm32and:
	cm32and %0,%29,%30
	.text
	.global cm32andn
cm32andn:
	cm32andn %0,%29,%30
	.text
	.global cm32or
cm32or:
	cm32or %0,%29,%30
	.text
	.global cm32ra
cm32ra:
	cm32ra %0,%29,%30
	.text
	.global cm32rd
cm32rd:
	cm32rd %29,%30
	.text
	.global cm32ri
cm32ri:	
	cm32ri %0,%29
	.text
	.global cm32rs
cm32rs:
	cm32rs %0,%29,%30
	.text
	.global cm32sa
cm32sa:
	cm32sa %0,%29,%30
	.text
	.global cm32sd
cm32sd:
	cm32sd %0,%29
	.text
	.global cm32si
cm32si:
	cm32si %0,%29
	.text
	.global cm32ss
cm32ss:
	cm32ss %0,%29,%30
	.text
	.global cm32xor
cm32xor:
	cm32xor %0,%29,%30
	.text
	.global cm64clr
cm64clr:
	cm64clr %0,%28
	.text
	.global cm64ra
cm64ra:
	cm64ra %0,%28,%30
	.text
	.global cm64rd
cm64rd:
	cm64rd %0,%28
	.text
	.global cm64ri
cm64ri:
	cm64ri %0,%28
	.text
	.global cm64ria2
cm64ria2:
	cm64ria2 %0,%28,%30
	.text
	.global cm64rs
cm64rs:
	cm64rs %0,%28,%30
	.text
	.global cm64sa
cm64sa:
	cm64sa %0,%28,%30
	.text
	.global cm64sd
cm64sd:
	cm64sd %0,%28
	.text
	.global cm64si
cm64si:
	cm64si %0,%28
	.text
	.global cm64sia2
cm64sia2:
	cm64sia2 %0,%28,%30
	.text
	.global cm64ss
cm64ss:
	cm64ss %0,%29,%30
	.text
	.global cm128ria2
cm128ria2:
	cm128ria2 %0,%29,%30
	.text
	.global cm128ria30
cm128ria3:
	cm128ria3 %0,%29,%30,0
	.text
	.global cm128ria4
cm128ria4:
	cm128ria4 %0,%29,%30,7
	.text
	.global cm128sia2
cm128sia2:
	cm128sia2 %0,%29,%30
	.text
	.global cm128sia3
cm128sia3:
	cm128sia3 %0,%29,%30,0
	.text
	.global cm128sia4
cm128sia4:
	cm128sia4 %0,%29,%30,7
	.text
	.global cm128vsa
cm128vsa:
	cm128vsa %0,%29,%30
	.text
	.global pkrli
pkrli:	
	pkrli %1,%31,%29,63
	.text
	.global pkrlic
pkrlic:	
	pkrlic %1,%31,%29,63
	.text
	.global pkrlih
pkrlih:	
	pkrlih %1,%31,%29,63
	.text
	.global pkrliu
pkrliu:	
	pkrliu %1,%31,%29,63
	.text
	.global rbi
rbi:		
	rbi %2,%29,%28,32
	.text
	.global rbil
rbil:		
	rbil %2,%29,%28,32
	.text
	.global rbir
rbir:		
	rbir %2,%29,%28,32
	.text
	.global wbi
wbi:		
	wbi %0,%1,%2,32
	.text
	.global wbic
wbic:		
	wbic %0,%1,%2,32
	.text
	.global wbiu
wbiu:		
	wbiu %0,%1,%2,32
			
