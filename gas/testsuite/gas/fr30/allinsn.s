 .data
foodata: .word 42
	 .text
footext:
	.global add
add:
	add r0, r1
	add #0, r2
	.global add2
add2:
	add2 #-1, r3
	.global addc
addc:
	addc r4, r5
	.global addn
addn:
	addn r6, r7
	addn #15, r8
	.global addn2
addn2:
	addn2 #-16, r9
	.global sub
sub:
	sub r10, r11
	.global subc
subc:
	subc r12, r13
	.global subn
subn:
	subn r14, r15
	.global cmp
cmp:
	cmp ac, fp
	cmp #1, sp
	.global cmp2
cmp2:
	cmp2 #-15, r0
	.global and
and:
	and r1, r2
	and r3, @r4
	.global andh
andh:
	andh r5, @r6
	.global andb
andb:
	andb r7, @r8
	.global or
or:
	or r9, r10
	or r11, @r12
	.global orh
orh:
	orh r13, @r14
	.global orb
orb:
	orb r15, @ac
	.global eor
eor:
	eor fp, sp
	eor r0, @r1
	.global eorh
eorh:
	eorh r2, @r3
	.global eorb
eorb:
	eorb r4, @r5
	.global bandl
bandl:
	bandl #15, @r6
	.global bandh
nadh:
	bandh #7, @r7
	.global borl
borl:
	borl #3, @r8
	.global borh
borh:
	borh #13, @r9
	.global beorl
beorl:
	beorl #15, @r10
	.global beorh
beorh:
	beorh #1, @r11
	.global btstl
btstl:
	btstl #0, @r12
	.global btsth
btsth:
	btsth #8, @r13
	.global mul
mul:
	mul r14, r15
	.global mulu
mulu:
	mulu ac, fp
	.global muluh
muluh:	
	muluh sp, r0
	.global mulh
mulh:	
	mulh r1, r2
	.global div0s
div0s:
	div0s r3
	.global div0u
div0u:
	div0u r4
	.global div1
div1:
	div1 r5
	.global div2
div2:
	div2 r6
	.global div3
div3:
	div3
	.global div4s
div4s:
	div4s
	.global lsl
lsl:
	lsl r7, r8
	lsl #3, r9
	.global lsl2
lsl2:
	lsl2 #0, r10
	.global lsr
lsr:
	lsr r11, r12
	lsr #15, r13
	.global lsr2
lsr2:
	lsr2 #15, r14
	.global asr
asr:
	asr r15, ac
	asr #6, fp
	.global asr2
asr2:
	asr2 #7, sp
	.global ldi_8
ldi_8:
	ldi:8 #0xff, r2
	.global ld
ld:
	ld @r3, r4
	ld @(R13, r5), r6
	ld @(R14, 0x1fc), r7
	ld @(R15, 0x3c), r8
	ld @r15+, r9
	ld @r15+, ps
	ld @R15+, tbr
	ld @r15+, rp
	ld @R15+, ssp
	.global lduh
lduh:
	lduh @r10, r11
	lduh @(r13, r12), r13
	lduh @(r14, #-256), r15
	.global ldub
ldub:
	ldub @ac, fp
	ldub @(r13, sp), r0
	ldub @(r14, -128), r1
	.global st
st:
	st r2, @r3
	st r4, @(r13, r5)
	st r6, @(r14, -512)
	st r7, @(r15, 0x3c)
	st r8, @ - r15
	st MDH, @-r15
	st PS, @ - r15
	.global lsth
sth:
	sth r9, @r10
	sth r11, @(r13, r12)
	sth r13, @(r14, 128)
	.global stb
stb:
	STB r14, @r15
	stb r0, @(r13, r1)
	STB r2, @(r14, -128)
	.global mov
mov:
	mov r3, r4
	MOV mdl, r5
	mov ps, r6
	mov r7, usp
	mov r8, ps
	.global jmp
jmp:
	jmp @r9
	.global ret
ret:
	ret
	.global bra
bra:
	bra footext
	.global bno
bno:
	bno footext
	.global beq
beq:
	beq footext
	.global bne
bne:
	bne footext
	.global bc
bc:
	bc footext
	.global bnc
bnc:
	bnc footext
	.global bn
bn:
	bn footext
	.global bp
bp:
	bp footext
	.global bv
bv:
	bv footext
	.global bnv
bnv:
	bnv footext
	.global blt
blt:
	blt footext
	.global bge
bge:
	bge footext
	.global ble
ble:
	ble footext
	.global bgt
bgt:
	bgt footext
	.global bls
bls:
	bls footext
	.global bhi
bhi:
	bhi footext
delay_footext:		
	.global jmp_d
jmp_d:
	jmp:d @r11
	nop
	.global ret_d
ret_d:
	ret:d
	nop
	.global bra_d
bra_d:
	bra:D delay_footext
	nop
	.global bno_d
bno_d:
	bno:d delay_footext
	nop
	.global beq_d
beq_d:
	beq:D delay_footext
	nop
	.global bne_d
bne_d:
	bne:d delay_footext
	nop
	.global bc_d
bc_d:
	bc:d delay_footext
	nop
	.global bnc_d
bnc_d:
	bnc:d delay_footext
	nop
	.global bn_d
bn_d:
	bn:d delay_footext
	nop
	.global bp_d
bp_d:
	bp:d delay_footext
	nop
	.global bv_d
bv_d:
	bv:d delay_footext
	nop
	.global bnv_d
bnv_d:
	bnv:d delay_footext
	nop
	.global blt_d
blt_d:
	blt:d delay_footext
	nop
	.global bge_d
bge_d:
	bge:d delay_footext
	nop
	.global ble_d
ble_d:
	ble:d delay_footext
	nop
	.global bgt_d
bgt_d:
	bgt:d delay_footext
	nop
	.global bls_d
bls_d:
	bls:d delay_footext
	nop
	.global bhi_d
bhi_d:
	bhi:d delay_footext
	nop
	.global ldres
ldres:
	ldres @r2+, #8
	.global stres
stres:
	stres #15, @r3+
	.global nop
nop:
	nop
	.global andccr
andccr:
	andccr #255
	.global orccr
orccr:
	orccr #125
	.global stilm
stilm:
	stilm #97
	.global addsp
addsp:
	addsp #-512
	.global extsb
extsb:
	extsb r9
	.global extub
extub:
	extub r10
	.global extsh
extsh:
	extsh r11
	.global extuh
extuh:
	extuh r12
	.global enter
enter:
	enter #1020
	.global leave
leave:
	leave 
	.global xchb
xchb:
	xchb @r14, r15
	.global ldi_32
ldi_32:
	ldi:32 #0x12345678, r0
	.global copop
copop:
	copop #15, #1, cr3, cr4
	copop #15, #4, cr5, cr6
	copop #15, #255, cr7, cr0
	.global copld
copld:
	copld #0, #0, r4, cr0
	.global copst
copst:
	copst #7, #2, cr1, r5
	.global copsv
copsv:
	copsv #8, #3, cr2, r6
	.global ldm0
ldm0:
	ldm0 (r0, r2, r3, r7)
	.global ldm1
ldm1:
	ldm1 (r8, r11, r15)
	.global stm0
stm0:
	stm0 (r2, r3)
	.global stm1
stm1:
	stm1 (r13, r14)
	.global call
call:
	call footext
	call @r10
	.global call_d
call_d:
	call:D footext
	nop
	call:d @r12
	nop
	.global dmov
dmov:
	dmov @0x88, r13
	dmov r13, @0x54
	dmov @0x44, @r13+
	dmov @R13+, @0x2
	dmov @0x2c, @-r15
	dmov @r15+, @38
	.global dmovh
dmovh:
	dmovh @0x88, r13
	dmovh r13, @0x52
	dmovh @0x34, @r13 +
	dmovh @r13+, @0x52
	.global dmovb
dmovb:
	dmovb @0x91, r13
	dmovb r13, @0x53
	dmovb @71, @r13+
	dmovb @r13+, @0x0
	.global ldi_20
ldi_20:
 	ldi:20 #0x000fffff, r1
finish:	
        ldi:32 #0x8000,r0
	mov    r0,ssp
        ldi:32 #1,r0
	int    #10
	.global inte
inte:
	inte
	.global reti
reti:
	reti
