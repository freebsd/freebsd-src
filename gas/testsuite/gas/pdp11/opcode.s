# Opcode test for PDP-11.
# Copyright 2002 Free Software Foundation, Inc.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

foo:	.word	0
bar:	.word	foo
	
start:	halt
start2:	wait
	rti
	bpt
	iot
	reset
	rtt
	mfpt
	jmp	(r1)+
	rts	r2
	spl	3
	nop
	clc
	clv
	clz
	cln
	ccc
	sec
	sev
	sez
	sen
	scc
	swab	pc
1:	br	1b
	bne	1b
	beq	1b
	bge	1b
	blt	1b
	bgt	1b
	ble	1b
	jsr	pc,@(sp)+
	clr	-(sp)
	com	r0
	inc	r1
	dec	r2
	neg	r3
	adc	r4
	sbc	r5
	tst	(sp)+
	ror	r5
	rol	r4
	asr	@10(r4)
	asl	4(r5)
	mark	2
	mfpi	sp
	mtpi	@$402
	sxt	r3
	csm	2(r4)
	tstset	(r3)
	wrtlck	2(r4)
	mov	r0,r1
	cmp	(r0),(r4)
	bit	(r0)+,-(r3)
	bic	foo,@bar
	bis	@(r1)+,@-(r5)
	add	4(r3),@6(r3)
	mul	$10,r2
	div	1b,r4
	ash	$3,r4
	ashc	$7,r2
	xor	r3,10(sp)
	fadd	r2
	fsub	r1
	fmul	r4
	fdiv	r0
	l2dr	r1
	movc
	movrc
	movtc
	locc
	skpc
	scanc
	spanc
	cmpc
	matc
	addn
	subn
	cmpn
	cvtnl
	cvtpn
	cvtnp
	ashn
	cvtln
	l3dr	r5
	addp
	subp
	cmpp
	cvtpl
	mulp
	divp
	ashp
	cvtlp
	movci
	movrci
	movtci
	locci
	skpci
	scanci
	spanci
	cmpci
	matci
	addni
	subni
	cmpni
	cvtnli
	cvtpni
	cvtnpi
	ashni
	cvtlni
	addpi
	subpi
	cmppi
	cvtpli
	mulpi
	divpi
	ashpi
	cvtlpi
	med
2:	xfc	42
	sob	r0,2b
	bpl	2b
	bmi	2b
	bhi	2b
	blos	2b
	bvc	2b
	bvs	2b
	bcc	2b
	bcs	2b
	emt	69
	sys	42
	clrb	(r3)
	comb	@-(r5)
	incb	@(sp)+
	decb	r3
	negb	foo
	adcb	@bar
	sbcb	-(r2)
	tstb	(r4)+
	rorb	r1
	rolb	r2
	asrb	r3
	aslb	r4
	mtps	$0340
	mfpd	sp
	mtpd	(r0)
	mfps	-(sp)
	movb	$17,foo
	cmpb	r1,(r2)
	bitb	$0117,r5
	bicb	$1,bar
	bisb	$2,@bar
	sub	r0,r5
	cfcc
	setf
	seti
	ldub
	setd
	setl
	ldfps	$1
	stfps	-(sp)
	stst	(r2)
	clrf	ac3
	tstf	ac1
	absf	ac2
	negf	ac0
	mulf	$0f0.25,ac1
	modf	ac5,ac0
	addf	foo,ac2
	ldf	@bar,ac1
	subf	ac4,ac3
	cmpf	ac5,ac2
	stf	ac1,-(sp)
	divf	$0f20.0,ac0
	stexp	ac2,r5
	stcfi	ac3,r0
	stcff	ac3,ac5
	ldexp	r0,ac2
	ldcif	r2,ac3
	ldcff	ac5,ac2

# aliases for some of these opcodes:

	l2d	r1
3:	l3d	r4
	bhis	3b
	blo	3b
	trap	99
	clrd	ac3
	tstd	ac2
	absd	ac1
	negd	ac0
	muld	ac5,ac2
	modd	ac4,ac0
	addd	ac4,ac3
	ldd	bar,ac0
	subd	foo,ac2
	cmpd	ac5,ac2
	std	ac1,(r2)
	divd	(sp)+,ac3
	stcfl	ac2,r5
	stcdi	ac3,r0
	stcdl	ac2,r4
	stcfd	ac2,ac5
	stcdf	ac1,ac4
	ldcid	r0,ac1
	ldclf	r4,ac2
	ldcld	$01234567,ac3
	ldcfd	ac5,ac2
	ldcdf	ac4,ac0
