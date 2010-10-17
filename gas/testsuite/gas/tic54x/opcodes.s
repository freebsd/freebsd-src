	;; opcodes tests
	.text
	.mmregs
	.global X, Y, Z
	.global _opcodes, _opcodes_end
	.label _opcodes_load
_opcodes:		
	abdst	*ar3+, *ar4+	
	abs	a		
	abs	a,b		
	add	*ar0+, a	; Smem, src
	add	*ar1+, ts, a	; Smem, TS, src 
	add	*ar2+, 16, a	; Smem, 16, src [,dst] 
	add	*ar3+, a, b	; Smem [,SHIFT], src [,dst] (-16<=SHIFT<=15)
	
	add	*ar4+, 1, a	; Xmem, SHFT, src (0<=SHFT<=15) 
	add	*ar3+, *ar4+, a	; Xmem, Ymem, dst
	add	#-32768, a	; #lk [,SHFT], src [,dst] (-32768<=lk<=32767)
	
	add	#0,16,a,b	; #lk, 16, src, [,dst]
	
	add	a,-16,b		; src [,SHIFT][,dst]
	add	a,asm,b		; src, ASM [,dst] 
	addc	*ar0+,a
	addm	#1,*ar1+	
	
	adds	*ar2+,a
	and	*ar3+,a		; Smem,src
	and	#1,1,a,b	; #lk[,SHFT],src[,dst]
	
	and	#1,#16,a,b	; #lk,16,src[,dst]
	
	and	a		; src[,SHIFT][,dst]
	andm	#1,*ar0+	
	
	b	_opcodes_end		
	
	bd	#_opcodes_end
        nop
        nop

	bacc	a		
	baccd	b
        nop
        nop

	banz	_opcodes_end,*ar1+	
	
	banzd	_opcodes_end,*ar2+	
        nop
        nop

	bc	_opcodes_end, AEQ,AOV	
	
	bcd	_opcodes_end, BIO,C,TC	
        nop
        nop

	bit	*ar3+,1		
	bitf	*ar4+,#-1	
	
	bitt	*ar5+		
	cala	a
	calad	b
        nop
        nop

        call	_opcodes_end
	
	calld	_opcodes_end		
        nop
        nop

	cc	_opcodes_end, tc	
	
	ccd	_opcodes_end, aeq	
        nop
        nop

	cmpl	b,a
	cmpm	*ar0+,#1	
	
	cmpr	1,ar1
	cmps	a,*ar2+		
	dadd	*ar3-, a, b
	dadst	*ar4-, a
	delay	*ar5+		
	dld	*ar6-, a
	drsub	*ar7-, b
	dsadt	*ar0-, a
	dst	a, *ar1-	
	dsub	*ar2-, b
	dsubt	*ar3-, a
	exp	a
	firs	*ar3+,*ar4+,_opcodes_end	
	
	frame	-128		
	idle	2		
	intr	15		
	ld	*ar0+,a			; Smem,dst
	ld	*ar1+,ts,a		; Smem,TS,dst
	ld	*ar2+,16,a		; Smem,16,dst
	ld	*ar3+,1,a		; Smem[,SHIFT],dst
	
	ld	*ar4+,1,a		; Xmem,SHFT,dst
	ld	#1,b			; #K,dst
	ld	#32767,1,a		; #lk,[,SHFT],dst
	
	ld	#32767,16,a		; #lk,16,dst
	
	ld	a,asm,b			; src,ASM[,dst]
	ld	a,1,b			; src[,SHIFT],dst
	ld	*ar0+,t		
	ld	*ar1+,dp	
	ld	#_opcodes_end,dp	; FIXME try to print label on disasm
					; note:	TI assembler doesn't shift 
					; the address encoding. 
	ld	#15,asm		
	ld	#7,arp		
	ld	*ar2+,asm	
	ldm	ar3,a		
	ld	*ar2+,a || mac	*ar3+,b	; single-line parallell
	ld	*ar4+,b	|| macr	*ar5+,a	; with optional DST_ specified
	ld	*ar2+,a			; double-line parallel
	|| mas	*ar3+		
	ld	*ar4+,b			; parallel spans
					; inserted line
	|| masr	*ar5+
	ldr	*ar6+,a
	ldu	*ar7+,a
	lms	*ar3+,*ar4+
	ltd	*ar0+		
	mac	*ar1+,a
	macr	*ar2+,a
	mac	*ar2+,*ar3+,a,b
	macr	*ar4+,*ar5+,a,b
	mac	#1,a,b
	
	mac	*ar0+,#1,a
	
	maca	*ar1+			; *ar6+,b (valid)
	maca	t,a,b		
	macd	*ar2+,_opcodes_end,a	
	
	macp	*ar3+,_opcodes_end,a	
	
	macsu	*ar4+,*ar5+,a
	mar	*ar6+
	mas	*ar7+,a
	masr	*ar0+,a
	mas	*ar3+,*ar4+,a,b
	masr	*ar2+,*ar5+,a,b
	masa	*ar6+		; *ar6+,b (valid)
	masa	t,a,b		
	masar	t,a		
	max	a
	min	b
	mpy	*ar7+,a
	mpy	*ar3+,*ar4+,b
	mpy	*ar0,#1,a
	
	mpy	#1,a		
	
	mpya	*ar0+
	mpya	b
	mpyu	*ar1+,b
	mvdd	*ar2+,*ar3+
	mvdk	*ar4+,X
	
	mvdm	X,ar5
	
	mvdp	*ar6+,_opcodes_end	
	
	mvkd	X,*ar7+
	
	mvmd	ar0,X
	
	mvmm	ar1,ar2
	mvpd	_opcodes_end,*ar3+	
	
	neg	a,b		
	
	nop			
	norm	a
	or	*ar0+,b
	or	#(3+4),b		
	
	or	#1,16,b	
	
	or	b
	orm	#1,*ar1+
	
	poly	*ar2+
	popd	*ar3+
	popm	ar4	
	portr	0,*ar5+		
	
	portw	*ar6+,0		
	
	pshd	*ar7+
	pshm	ar0	
	rc	ANEQ		
	rcd	AGT		
	reada	*ar1+
	reset			
	ret			
	retd			
        nop
        nop
	rete
	reted			
        nop
        nop
        retf
	retfd			
	rol	a
	roltc	a
	ror	b
	rpt	*ar0+		
	nop
	rpt	#32		
	nop
	rpt	#65535		
	nop
	rptb	_opcodes_end-1		
	nop
	rptbd	_opcodes_end-1		
	nop
	nop
	rptz	a,#32767	
	nop
	rsbx	1,15		
	saccd	a,*ar3+,ALT	
	sat	a
	sfta	a,15,b
	sftc	a
	sftl	a,15
	sqdst	*ar2+,*ar3+
	squr	*ar4+,b
	squr	a,a		
	squra	*ar5+,a
	squrs	*ar6+,a
	srccd	*ar2+,ALEQ	
	ssbx	1,15		
	st	t,*ar0+		
	st	trn,*ar1+	
	st	#32767,*ar2+
	
	sth	a,*ar3+		
	sth	a,asm,*ar4+	
	sth	a,15,*ar5+	
	sth	a,-16,*ar6+	
	
	stl	a,*ar7+		
	stl	a,asm,*ar0+	
	stl	a,15,*ar1+	
	stl	a,15,*ar2+	
	
	stlm	a,ar3
	stm	#32767,ar4
	
	st	a,*ar5+		
	|| add	*ar4+,b		
	st	a,*ar3+
	|| ld	*ar2+,b		
	st	a,*ar3+
	|| ld	*ar4+,t		
	st	a,*ar5+
	|| mac	*ar2+,b		
	st	a,*ar3+
	|| masr	*ar4+,b		
	st	a,*ar3+
	|| mpy	*ar4+,b		
	st	a,*ar3+
	|| sub	*ar4+,b		
	strcd	*ar5+,BEQ	
	sub	*ar0+,a		
	sub	*ar1+,ts,a	
	sub	*ar2+,16,a,b
	sub	*ar3+,a,b	
	
	sub	*ar4+,15,a	
	sub	*ar5+,*ar4+,b	
	sub	#1,15,a,b
	
	sub	#1,16,a,b	
	
	sub	a,-16,b
	sub	a,asm,b		
	subb	*ar0+,a
	subc	*ar1+,a
	subs	*ar2+,a
	trap	15		
	writa	*ar3+		
	xc	1,AOV		
	xor	*ar4+,a
	xor	#1,a		
	
	xor	#1,16,a		
	
	xor	a,1,b
	xorm	#1,*ar5+	
_opcodes_end:	
	.data
X:	.word	0	
Y:	.word	1	
*	.word	Z
	.end	

