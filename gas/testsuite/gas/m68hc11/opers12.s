
#
# Try to verify all operand modes for 68HC12
#
	sect .text
	globl start

start:
	anda	[12,x]		; Indexed indirect
	ldaa	#10
	ldx	L1
L1:	ldy	,x
	addd	1,y		; Offset from register
	subd	-1,y
	eora	15,y
	eora	-16,y
	eorb	16,y
	eorb	-17,y
	oraa	128,sp
	orab	-128,sp
	orab	255,x
	orab	-256,x
	anda	256,x
	andb	-257,x
	anda	[12,x]		; Indexed indirect (16-bit offset)
	ldaa	[257,y]
	ldab	[32767,sp]
	ldd	[32768,pc]
	ldd	L1,pc
	std	a,x		; Two-reg index
	ldx	b,x
	stx	d,y
	addd	1,+x		; Pre-Auto inc
	addd	2,+x
	addd	8,+x
	addd	1,sp+		; Post-Auto inc
	addd	2,sp+
	addd	8,sp+
	subd	1,-y		; Pre-Auto dec
	subd	2,-y
	subd	8,-y
	addd	1,y-		; Post-Auto dec
	addd	2,y-
	addd	8,y-
	std	[d,x]		; Indexed indirect with two reg index
	std	[d,y]
	std	[d,sp]
	std	[d,pc]
	beq	L1
	lbeq	start
	lbcc	L2
;;
;; Move insn with various operands
;; 
	movb	start, 1,x
	movw	1,x, start
	movb	start, 1,+x
	movb	start, 1,-x
	movb	#23, 1,-sp
	movb	L1, L2
	movb	L1, a,x
	movw	L1, b,x
	movw	L1, d,x
	movw	d,x, a,x
	movw	b,sp, d,pc
	movw	b,sp, L1
	movw	b,sp, 1,x
	movw	d,x, a,y
	trap	#0x30
	trap	#0x39
	trap	#0x40
	trap	#0x80
	trap	#255
L2:	
	movw 1,x,2,x
	movw -1,-1
	movw -1,1,x
	movw #-1,1,x
	movw 3,8
	movw #3,3
	movw #3,1,x
	movw 3,1,x
	movw 3,+2,x
	movw 4,-2,x
	rts
;;
;; Post-index byte with relocation
;; 
post_indexed_pb:
t1:
	leas	abort,x
t2:
	leax	t2-t1,y
	leax	toto,x
	leas	toto+titi,sp
	leay	titi,x
	leas	bb,y
	leas	min5b,pc
	leas	max5b,pc
	leas	min9b,pc
	leas	max9b,pc

;;
;; Disassembler bug with movb
;;
	movb	#23,0x2345
	movb	#40,12,sp
	movb	#39,3,+sp
	movb	#20,14,sp
	movw	#0x3210,0x3456
	movw	#0x4040,12,sp
	movw	#0x3900,3,+sp
	movw	#0x2000,14,sp
#	movb	#111,start

titi = 10
toto = 100
min5b= -16
max5b= 15
min9b= -256
max9b= 255
bb = 10240
