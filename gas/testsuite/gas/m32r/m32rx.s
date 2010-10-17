# Test new instructions
branchpoint:
	
	.text
	.global bcl
bcl:
	bcl branchpoint

	.text
	.global bncl
bncl:
	bncl branchpoint

	.text
	.global cmpz
cmpz:
	cmpz fp

	.text
	.global cmpeq
cmpeq:
	cmpeq fp, fp

	.text
	.global maclh1
maclh1:
	maclh1 fp, fp
	
	.text
	.global macsl0
msblo:
	msblo fp, fp
	
	.text
	.global mulwu1
mulwu1:
	mulwu1 fp, fp
	
	.text
	.global macwu1
macwu1:
	macwu1 fp, fp
	
	.text
	.global sadd
sadd:
	sadd
	
	.text
	.global satb
satb:
	satb fp, fp

	
	.text
	.global mulhi
mulhi:
	mulhi fp, fp, a1
	
	.text
	.global mullo
mullo:
	mullo fp, fp, a0
	
	.text
	.global divh
divh:
	divh fp, fp
	
	.text
	.global machi
machi:
	machi fp, fp, a1
	
	.text
	.global maclo
maclo:
	maclo fp, fp, a0
	
	.text
	.global mvfachi
mvfachi:
	mvfachi fp, a1
	
	.text
	.global mvfacmi
mvfacmi:
	mvfacmi fp, a1
	
	.text
	.global mvfaclo
mvfaclo:
	mvfaclo fp, a1
	
	.text
	.global mvtachi
mvtachi:
	mvtachi fp, a1
	
	.text
	.global mvtaclo
mvtaclo:
	mvtaclo fp, a0
	
	.text
	.global rac
rac:
	rac a1
	
	.text
	.global rac_ds
rac_ds:
	rac a1, a0
	
	.text
	.global rac_dsi
rac_dsi:
	rac a0, a1, #1
	
	.text
	.global rach
rach:
	rach a1
	
	.text
	.global rach_ds
rach_ds:
	rach a0, a1
	
	.text
	.global rach_dsi
rach_dsi:
	rach a1, a0, #2
	
# Test explicitly parallel and implicitly parallel instructions
# Including apparent instruction sequence reordering.
	.text
	.global bc__add
bc__add:
	bc bcl || add fp, fp
# Use bc.s here as bc is relaxable and thus a nop will be emitted.
	bc.s bcl
	add fp, fp

	.text
	.global bcl__addi
bcl__addi:	
	bcl bcl || addi fp, #77
	addi fp, #77
# Use bcl.s here as bcl is relaxable and thus the parallelization won't happen.
	bcl.s bcl

	.text
	.global bl__addv
bl__addv:
	bl bcl || addv fp, fp
	addv fp, fp
# Use bl.s here as bl is relaxable and thus the parallelization won't happen.
	bl.s bcl
	
	.text
	.global bnc__addx
bnc__addx:
	bnc bcl || addx fp, fp
# Use bnc.s here as bnc is relaxable and thus the parallelization attempt won't
# happen.  Things still won't be parallelized, but we want this test to try.
	bnc.s bcl
	addx fp, fp

	.text
	.global bncl__and
bncl__and:
	bncl bcl || and fp, fp
	and fp, fp
	bncl.s bcl

	.text
	.global bra__cmp
bra__cmp:
	bra bcl || cmp fp, fp
	cmp fp, fp
# Use bra.s here as bra is relaxable and thus the parallelization won't happen.
	bra.s bcl
	
	.text
	.global jl__cmpeq
jl__cmpeq:
	jl fp || cmpeq fp, fp
	cmpeq fp, fp
	jl fp
	
	.text
	.global jmp__cmpu
jmp__cmpu:
	jmp fp || cmpu fp, fp
	cmpu fp, fp
	jmp fp
	
	.text
	.global ld__cmpz
ld__cmpz:
	ld fp, @fp || cmpz r1
	cmpz r1
	ld fp, @fp 
	
	.text
	.global ld__ldi
ld__ldi:
	ld fp, @r1+ || ldi r2, #77
	ld fp, @r1+ 
	ldi r2, #77
	
	.text
	.global ldb__mv
ldb__mv:
	ldb fp, @fp || mv r2, fp
	ldb fp, @fp 
	mv r2, fp

	.text
	.global ldh__neg
ldh__neg:
	ldh fp, @fp || neg r2, fp
	ldh fp, @fp
	neg r2, fp

	.text
	.global ldub__nop
ldub__nop:
	ldub fp, @fp || nop
	ldub fp, @fp 
	nop

	.text
	.global lduh__not
lduh__not:
	lduh fp, @fp || not r2, fp
	lduh fp, @fp
	not r2, fp

	.text
	.global lock__or
lock__or:
	lock fp, @fp || or r2, fp
	lock fp, @fp
	or r2, fp

	.text
	.global mvfc__sub
mvfc__sub:
	mvfc fp, cr1 || sub r2, fp
	mvfc fp, cr1
	sub r2, fp

	.text
	.global mvtc__subv
mvtc__subv:
	mvtc fp, cr2 || subv r2, fp
	mvtc fp, cr2
	subv r2, fp

	.text
	.global rte__subx
rte__subx:
	rte || sub r2, fp
	rte
	subx r2, fp

	.text
	.global sll__xor
sll__xor:
	sll fp, r1 || xor r2, fp
	sll fp, r1
	xor r2, fp

	.text
	.global slli__machi
slli__machi:
	slli fp, #22 || machi r2, fp
	slli fp, #22
	machi r2, fp

	.text
	.global sra__maclh1
sra__maclh1:
	sra fp, fp || maclh1 r2, fp
	sra fp, fp
	maclh1 r2, fp

	.text
	.global srai__maclo
srai__maclo:
	srai fp, #22 || maclo r2, fp
	srai fp, #22
	maclo r2, fp

	.text
	.global srl__macwhi
srl__macwhi:
	srl fp, fp || macwhi r2, fp
	srl fp, fp
	macwhi r2, fp

	.text
	.global srli__macwlo
srli__macwlo:
	srli fp, #22 || macwlo r2, fp
	srli fp, #22
	macwlo r2, fp
	
	.text
	.global st__macwu1
st__macwu1:
	st fp, @fp || macwu1 r2, fp
	st fp, @fp
	macwu1 r2, fp

	.text
	.global st__msblo
st__msblo:
	st fp, @+fp || msblo r2, fp
	st fp, @+fp 
	msblo r2, fp

	.text
	.global st__mul
st__mul:
	st fp, @-fp || mul r2, fp
	st fp, @-fp
	mul r2, fp

	.text
	.global stb__mulhi
stb__mulhi:
	stb fp, @fp || mulhi r2, fp
	stb fp, @fp
	mulhi r2, fp
	
	.text
	.global sth__mullo
sth__mullo:
	sth fp, @fp || mullo r2, fp
	sth fp, @fp 
	mullo r2, fp

	.text
	.global trap__mulwhi
trap__mulwhi:
	trap #2 || mulwhi r2, fp
	trap #2
	mulwhi r2, fp

	.text
	.global unlock__mulwlo
unlock__mulwlo:
	unlock fp, @fp || mulwlo r2, fp
	unlock fp, @fp
	mulwlo r2, fp

	.text
	.global add__mulwu1
add__mulwu1:
	add fp, fp || mulwu1 r2, fp
	add fp, fp
	mulwu1 r2, fp

	.text
	.global addi__mvfachi
addi__mvfachi:
	addi fp, #77 || mvfachi r2, a0
	addi fp, #77
	mvfachi r2, a0

	.text
	.global addv__mvfaclo
addv__mvfaclo:
	addv fp, fp || mvfaclo r2, a1
	addv fp, fp
	mvfaclo r2, a1

	.text
	.global addx__mvfacmi
addx__mvfacmi:
	addx fp, fp || mvfacmi r2, a0
	addx fp, fp
	mvfacmi r2, a0

	.text
	.global and__mvtachi
and__mvtachi:
	and fp, fp || mvtachi r2, a0
	and fp, fp
	mvtachi r2, a0
	
	.text
	.global cmp__mvtaclo
cmp__mvtaclo:
	cmp fp, fp || mvtaclo r2, a0
	cmp fp, fp
	mvtaclo r2, a0

	.text
	.global cmpeq__rac
cmpeq__rac:
	cmpeq fp, fp || rac a1
	cmpeq fp, fp
	rac a1

	.text
	.global cmpu__rach
cmpu__rach:
	cmpu fp, fp || rach a0, a1
	cmpu fp, fp
	rach a1, a1, #1

	.text
	.global cmpz__sadd
cmpz__sadd:
	cmpz fp || sadd
	cmpz fp
	sadd


	
# Test private instructions	
	.text
	.global sc
sc:
	sc
	sadd
	
	.text
	.global snc
snc:
	snc
	sadd 
	
	.text
	.global jc
jc:
	jc fp
	
	.text
	.global jnc
jnc:
	jnc fp
		
	.text
	.global pcmpbz
pcmpbz:
	pcmpbz fp
	
	.text
	.global sat
sat:
	sat fp, fp
	
	.text
	.global sath
sath:
	sath fp, fp 


# Test parallel versions of the private instructions
	
	.text
	.global jc__pcmpbz
jc__pcmpbz:
	jc fp || pcmpbz fp
	jc fp 
	pcmpbz fp
	
	.text
	.global jnc__ldi
jnc__ldi:
	jnc fp || ldi fp, #77
	jnc fp
	ldi fp, #77
	
	.text
	.global sc__mv
sc__mv:
	sc || mv fp, r2
	sc 
	mv fp, r2

	.text
	.global snc__neg
snc__neg:
	snc || neg fp, r2
	snc 
	neg fp, r2
	
# Test automatic and explicit parallelisation of instructions
	.text
	.global nop__sadd
nop__sadd:
	nop
	sadd

	.text
	.global sadd__nop
sadd__nop:
	sadd
	nop

	.text
	.global sadd__nop_reverse
sadd__nop_reverse:
	sadd || nop

	.text
	.global add__not
add__not:
	add  r0, r1
	not  r3, r5

	.text
	.global add__not__dest_clash
add__not_dest_clash:
	add  r3, r4
	not  r3, r5

	.text
	.global add__not__src_clash
add__not__src_clash:
	add  r3, r4
	not  r5, r3

	.text
	.global add__not__no_clash
add__not__no_clash:
	add  r3, r4
	not  r4, r5

	.text
	.global mul__sra
mul__sra:
	mul  r1, r2
	sra  r3, r4
	
	.text
	.global mul__sra__reverse_src_clash
mul__sra__reverse_src_clash:
	mul  r1, r3
	sra  r3, r4
	
	.text
	.global bc__add_
bc__add_:
	bc.s label
	add r1, r2

	.text
	.global add__bc
add__bc:
	add r3, r4
	bc.s  label

	.text
	.global bc__add__forced_parallel
bc__add__forced_parallel:
	bc label || add r5, r6

	.text
	.global add__bc__forced_parallel
add__bc__forced_parallel:
	add r7, r8 || bc label
label:
	nop

; Additional testcases.
; These insns were added to the chip later.

	.text
mulwhi:
	mulwhi fp, fp, a0
	mulwhi fp, fp, a1
	
mulwlo:
	mulwlo fp, fp, a0
	mulwlo fp, fp, a1

macwhi:
	macwhi fp, fp, a0
	macwhi fp, fp, a1

macwlo:
	macwlo fp, fp, a0
	macwlo fp, fp, a1
