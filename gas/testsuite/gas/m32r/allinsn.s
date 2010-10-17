 .data
foodata: .word 42
 .text
footext:
	.text
	.global add
add:
	add fp,fp
	.text
	.global add3
add3:
	add3 fp,fp,#0
	.text
	.global and
and:
	and fp,fp
	.text
	.global and3
and3:
	and3 fp,fp,#0
	.text
	.global or
or:
	or fp,fp
	.text
	.global or3
or3:
	or3 fp,fp,#0
	.text
	.global xor
xor:
	xor fp,fp
	.text
	.global xor3
xor3:
	xor3 fp,fp,#0
	.text
	.global addi
addi:
	addi fp,#0
	.text
	.global addv
addv:
	addv fp,fp
	.text
	.global addv3
addv3:
	addv3 fp,fp,#0
	.text
	.global addx
addx:
	addx fp,fp
	.text
	.global bc8
bc8:
	bc footext
	.text
	.global bc8_s
bc8_s:
	bc.s footext
	.text
	.global bc24
bc24:
	bc footext
	.text
	.global bc24_l
bc24_l:
	bc.l footext
	.text
	.global beq
beq:
	beq fp,fp,footext
	.text
	.global beqz
beqz:
	beqz fp,footext
	.text
	.global bgez
bgez:
	bgez fp,footext
	.text
	.global bgtz
bgtz:
	bgtz fp,footext
	.text
	.global blez
blez:
	blez fp,footext
	.text
	.global bltz
bltz:
	bltz fp,footext
	.text
	.global bnez
bnez:
	bnez fp,footext
	.text
	.global bl8
bl8:
	bl footext
	.text
	.global bl8_s
bl8_s:
	bl.s footext
	.text
	.global bl24
bl24:
	bl footext
	.text
	.global bl24_l
bl24_l:
	bl.l footext
	.text
	.global bnc8
bnc8:
	bnc footext
	.text
	.global bnc8_s
bnc8_s:
	bnc.s footext
	.text
	.global bnc24
bnc24:
	bnc footext
	.text
	.global bnc24_l
bnc24_l:
	bnc.l footext
	.text
	.global bne
bne:
	bne fp,fp,footext
	.text
	.global bra8
bra8:
	bra footext
	.text
	.global bra8_s
bra8_s:
	bra.s footext
	.text
	.global bra24
bra24:
	bra footext
	.text
	.global bra24_l
bra24_l:
	bra.l footext
	.text
	.global cmp
cmp:
	cmp fp,fp
	.text
	.global cmpi
cmpi:
	cmpi fp,#0
	.text
	.global cmpu
cmpu:
	cmpu fp,fp
	.text
	.global cmpui
cmpui:
	cmpui fp,#0
	.text
	.global div
div:
	div fp,fp
	.text
	.global divu
divu:
	divu fp,fp
	.text
	.global rem
rem:
	rem fp,fp
	.text
	.global remu
remu:
	remu fp,fp
	.text
	.global jl
jl:
	jl fp
	.text
	.global jmp
jmp:
	jmp fp
	.text
	.global ld
ld:
	ld fp,@fp
	.text
	.global ld_2
ld_2:
	ld fp,@(fp)
	.text
	.global ld_d
ld_d:
	ld fp,@(0,fp)
	.text
	.global ld_d2
ld_d2:
	ld fp,@(fp,0)
	.text
	.global ldb
ldb:
	ldb fp,@fp
	.text
	.global ldb_2
ldb_2:
	ldb fp,@(fp)
	.text
	.global ldb_d
ldb_d:
	ldb fp,@(0,fp)
	.text
	.global ldb_d2
ldb_d2:
	ldb fp,@(fp,0)
	.text
	.global ldh
ldh:
	ldh fp,@fp
	.text
	.global ldh_2
ldh_2:
	ldh fp,@(fp)
	.text
	.global ldh_d
ldh_d:
	ldh fp,@(0,fp)
	.text
	.global ldh_d2
ldh_d2:
	ldh fp,@(fp,0)
	.text
	.global ldub
ldub:
	ldub fp,@fp
	.text
	.global ldub_2
ldub_2:
	ldub fp,@(fp)
	.text
	.global ldub_d
ldub_d:
	ldub fp,@(0,fp)
	.text
	.global ldub_d2
ldub_d2:
	ldub fp,@(fp,0)
	.text
	.global lduh
lduh:
	lduh fp,@fp
	.text
	.global lduh_2
lduh_2:
	lduh fp,@(fp)
	.text
	.global lduh_d
lduh_d:
	lduh fp,@(0,fp)
	.text
	.global lduh_d2
lduh_d2:
	lduh fp,@(fp,0)
	.text
	.global ld_plus
ld_plus:
	ld fp,@fp+
	.text
	.global ld24
ld24:
	ld24 fp,foodata
	.text
	.global ldi8
ldi8:
	ldi fp,0
	.text
	.global ldi16
ldi16:
	ldi fp,256
	.text
	.global lock
lock:
	lock fp,@fp
	.text
	.global machi
machi:
	machi fp,fp
	.text
	.global maclo
maclo:
	maclo fp,fp
	.text
	.global macwhi
macwhi:
	macwhi fp,fp
	.text
	.global macwlo
macwlo:
	macwlo fp,fp
	.text
	.global mul
mul:
	mul fp,fp
	.text
	.global mulhi
mulhi:
	mulhi fp,fp
	.text
	.global mullo
mullo:
	mullo fp,fp
	.text
	.global mulwhi
mulwhi:
	mulwhi fp,fp
	.text
	.global mulwlo
mulwlo:
	mulwlo fp,fp
	.text
	.global mv
mv:
	mv fp,fp
	.text
	.global mvfachi
mvfachi:
	mvfachi fp
	.text
	.global mvfaclo
mvfaclo:
	mvfaclo fp
	.text
	.global mvfacmi
mvfacmi:
	mvfacmi fp
	.text
	.global mvfc
mvfc:
	mvfc fp,psw
	.text
	.global mvtachi
mvtachi:
	mvtachi fp
	.text
	.global mvtaclo
mvtaclo:
	mvtaclo fp
	.text
	.global mvtc
mvtc:
	mvtc fp,psw
	.text
	.global neg
neg:
	neg fp,fp
	.text
	.global nop
nop:
	nop
	.text
	.global not
not:
	not fp,fp
	.text
	.global rac
rac:
	.text
	.global rach
rach:
	.text
	.global rte
rte:
	.text
	.global seth
seth:
	seth fp,0
	.text
	.global sll
sll:
	sll fp,fp
	.text
	.global sll3
sll3:
	sll3 fp,fp,0
	.text
	.global slli
slli:
	slli fp,0
	.text
	.global sra
sra:
	sra fp,fp
	.text
	.global sra3
sra3:
	sra3 fp,fp,0
	.text
	.global srai
srai:
	srai fp,0
	.text
	.global srl
srl:
	srl fp,fp
	.text
	.global srl3
srl3:
	srl3 fp,fp,0
	.text
	.global srli
srli:
	srli fp,0
	.text
	.global st
st:
	st fp,@fp
	.text
	.global st_2
st_2:
	st fp,@(fp)
	.text
	.global st_d
st_d:
	st fp,@(0,fp)
	.text
	.global st_d2
st_d2:
	st fp,@(fp,0)
	.text
	.global stb
stb:
	stb fp,@fp
	.text
	.global stb_2
stb_2:
	stb fp,@(fp)
	.text
	.global stb_d
stb_d:
	stb fp,@(0,fp)
	.text
	.global stb_d2
stb_d2:
	stb fp,@(fp,0)
	.text
	.global sth
sth:
	sth fp,@fp
	.text
	.global sth_2
sth_2:
	sth fp,@(fp)
	.text
	.global sth_d
sth_d:
	sth fp,@(0,fp)
	.text
	.global sth_d2
sth_d2:
	sth fp,@(fp,0)
	.text
	.global st_plus
st_plus:
	st fp,@+fp
	.text
	.global st_minus
st_minus:
	st fp,@-fp
	.text
	.global sub
sub:
	sub fp,fp
	.text
	.global subv
subv:
	subv fp,fp
	.text
	.global subx
subx:
	subx fp,fp
	.text
	.global trap
trap:
	trap 0
	.text
	.global unlock
unlock:
	unlock fp,@fp
	.text
	.global push
push:
	push fp
	.text
	.global pop
pop:
	pop fp
