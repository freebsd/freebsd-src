! Test recognition of predefined control register names, lower and upper
! case; getcon and putcon.  Exhaustive test in those domain is small and
! simple enough.  Note that basic-1.s has already tested non-predefined
! register names.

	.mode SHmedia
	.text
start:
	getcon sr,r21
	getcon ssr,r31
	getcon pssr,r22
	getcon intevt,r21
	getcon expevt,r21
	getcon pexpevt,r21
	getcon tra,r12
	getcon spc,r21
	getcon pspc,r41
	getcon resvec,r21
	getcon vbr,r19
	getcon tea,r21
	getcon dcr,r35
	getcon kcr0,r21
	getcon kcr1,r21
	getcon ctc,r22
	getcon usr,r21

	getcon SR,r2
	getcon SSR,r21
	getcon PSSR,r21
	getcon INTEVT,r21
	getcon EXPEVT,r38
	getcon PEXPEVT,r21
	getcon TRA,r21
	getcon SPC,r1
	getcon PSPC,r21
	getcon RESVEC,r21
	getcon VBR,r47
	getcon TEA,r21
	getcon DCR,r21
	getcon KCR0,r35
	getcon KCR1,r21
	getcon CTC,r21
	getcon USR,r21

	putcon r21,sr
	putcon r31,ssr
	putcon r22,pssr
	putcon r21,intevt
	putcon r21,expevt
	putcon r21,pexpevt
	putcon r12,tra
	putcon r21,spc
	putcon r41,pspc
	putcon r21,resvec
	putcon r19,vbr
	putcon r21,tea
	putcon r35,dcr
	putcon r21,kcr0
	putcon r21,kcr1
	putcon r22,ctc
	putcon r21,usr

	putcon r2,SR
	putcon r21,SSR
	putcon r21,PSSR
	putcon r21,INTEVT
	putcon r38,EXPEVT
	putcon r21,PEXPEVT
	putcon r21,TRA
	putcon r1,SPC
	putcon r21,PSPC
	putcon r21,RESVEC
	putcon r47,VBR
	putcon r21,TEA
	putcon r21,DCR
	putcon r35,KCR0
	putcon r21,KCR1
	putcon r21,CTC
	putcon r21,USR
