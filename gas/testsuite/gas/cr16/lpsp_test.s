        .text
        .global main
main:
	################
	# lpr reg, preg
	################
	lpr	r1,psr
	lpr	r2,cfg
	lpr	r2,intbasel
	lpr	r3,intbaseh
	lpr	r4,ispl
	lpr	r5,isph
	lpr	r6,uspl
	lpr	r7,usph
	lpr	r8,dsr
	lpr	r9,dcrl
	lpr	r10,dcrh
	lpr	r11,car0l
	lpr	r0,car0h
	lpr	r1,car1l
	lpr	r3,car1h
	#################
	# lprd regp, preg
	#################
	lprd	(r1,r0),psr
	lprd	(r2,r1),cfg
	lprd	(r3,r2),intbase
	lprd	(r4,r3),isp
	lprd	(r5,r4),usp
	lprd	(r6,r5),dsr
	lprd	(r7,r6),dcr
	lprd	(r8,r7),car0
	lprd	(r9,r8),car1
	#################
	# spr preg, reg
	#################
	spr	psr,r0
	spr	cfg,r1
	spr	intbasel,r2
	spr	intbaseh,r3
	spr	ispl,r4
	spr	isph,r5
	spr	uspl,r6
	spr	usph,r7
	spr	dsr,r8
	spr	dcrl,r9
	spr	dcrh,r10
	spr	car0l,r11
	spr	car0h,r0
	spr	car1l,r1
	spr	car1h,r2
	#################
	# sprd preg, regp
	#################
	sprd	psr,(r1,r0)
	sprd	cfg,(r2,r1)
	sprd	intbase,(r3,r2)
	sprd	isp,(r4,r3)
	sprd	usp,(r5,r4)
	sprd	dsr,(r6,r5)
	sprd	dcr,(r7,r6)
	sprd	car0,(r8,r7)
	sprd	car1,(r9,r8)
