 .data
foodata: .word 42
 .text
footext:
	.text
	.global add
add:
	add  R0,R0,R0
	.text
	.global addu
addu:
	addu R0,R0,R0
	.text
	.global addi
addi:
	addi R0,R0,#0
	.text
	.global addui
addui:
	addui R0,R0,#0
	.text
	.global sub
sub:
	sub R0,R0,R0
	.text
	.global subu
subu:
	subu R0,R0,R0
	.text
	.global subi
subi:
	subi R0,R0,#0
	.text
	.global subui
subui:
	subui R0,R0,#0
	.text
	.global and
and:
	and R0,R0,R0
	.text
	.global andi
andi:
	andi R0,R0,#0
	.text
	.global or
or:
	or R0,R0,R1
	.text
	.global ori
ori:
	ori R0,R0,#0
	.text
	.global xor
xor:
	xor R0,R0,R0
	.text
	.global xori
xori:
	xori R0,R0,#0
	.text
	.global nand
nand:
	nand R0,R0,R0
	.text
	.global nandi
nandi:
	nandi R0,R0,#0
	.text
	.global nor
nor:
	nor R0,R0,R0
	.text
	.global nori
nori:
	nori R0,R0,#0
	.text
	.global xnor
xnor:
	xnor R0,R0,R0
	.text
	.global xnori
xnori:
	xnori R0,R0,#0
	.text
	.global ldui
ldui:
	ldui R0,#0
	.text
	.global lsl
lsl:
	lsl R0,R0,R0
	.text
	.global lsli
lsli:
	lsli R0,R0,#0
	.text
	.global lsr
lsr:
	lsr R0,R0,R0
	.text
	.global lsri
lsri:
	lsri R0,R0,#0
	.text
	.global asr
asr:
	asr R0,R0,R0
	.text
	.global asri
asri:
	asri R0,R0,#0
	.text
	.global brlt
brlt:
	brlt R0,R0,0
	.text
	.global brle
brle:
	brle R0,R0,0
	.text
	.global breq
breq:
	breq R0,R0,0
	.text
	.global jmp
jmp:
	jmp 0
	.text
	.global jal
jal:
	jal R0,R0
	.text
	.global ei
ei:
	ei
	.text
	.global di
di:
	di
	.text
	.global reti
reti:
	reti R0
	.text
	.global ldw
ldw:
	ldw R0,R0,#0
	.text
	.global stw
stw:
	stw R0,R0,#0
	.text
	.global si
si:
	si R0
	.global brne
brne:
	brne R0,R0,0
	.global break
break:
	break
	.text
	.global nop
nop:
	nop
