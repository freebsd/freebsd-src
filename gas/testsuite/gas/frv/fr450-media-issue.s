	; M-1 first
	mand.p     fr0,fr1,fr2	; M1
	mpackh     fr4,fr5,fr6  ; M1 -- ok
	mand.p     fr0,fr1,fr2	; M1
	mcpli      fr4,#1,fr6	; M2 -- error
	mand.p     fr0,fr1,fr2	; M1
	mmulhu     fr4,fr6,acc8 ; M3 -- ok
	mand.p     fr0,fr1,fr2	; M1
	mqmulhu    fr4,fr6,acc8 ; M4 -- error
	mand.p     fr0,fr1,fr2	; M1
	mcuti      acc8,#2,fr8	; M5 -- ok
	mand.p     fr0,fr1,fr2	; M1
	mdcutssi   acc8,#2,fr8	; M6 -- error

	; M-2 first
	mqaddhss.p fr0,fr2,fr2	; M2
	mpackh     fr4,fr5,fr6  ; M1 -- error
	mqaddhss.p fr0,fr2,fr2	; M2
	mcpli      fr4,#1,fr6	; M2 -- error
	mqaddhss.p fr0,fr2,fr2	; M2
	mmulhu     fr4,fr6,acc8 ; M3 -- error
	mqaddhss.p fr0,fr2,fr2	; M2
	mqmulhu    fr4,fr6,acc8 ; M4 -- error
	mqaddhss.p fr0,fr2,fr2	; M2
	mcuti      acc8,#2,fr8	; M5 -- error
	mqaddhss.p fr0,fr2,fr2	; M2
	mdcutssi   acc8,#2,fr8	; M6 -- error

	; M-3 first
	mwtacc.p   fr0,acc0	; M3
	mpackh     fr4,fr5,fr6  ; M1 -- ok
	mwtacc.p   fr0,acc0	; M3
	mcpli      fr4,#1,fr6	; M2 -- error
	mwtacc.p   fr0,acc0	; M3
	mmulhu     fr4,fr6,acc8 ; M3 -- ok
	mwtacc.p   fr0,acc0	; M3
	mqmulhu    fr4,fr6,acc8 ; M4 -- error
	mwtacc.p   fr0,acc0	; M3
	mcuti      acc8,#2,fr8	; M5 -- ok
	mwtacc.p   fr0,acc0	; M3
	mdcutssi   acc8,#2,fr8	; M6 -- error

	; M-4 first
	mqcpxrs.p  fr0,fr2,acc0	; M4
	mpackh     fr4,fr5,fr6  ; M1 -- error
	mqcpxrs.p  fr0,fr2,acc0	; M4
	mcpli      fr4,#1,fr6	; M2 -- error
	mqcpxrs.p  fr0,fr2,acc0	; M4
	mmulhu     fr4,fr6,acc8 ; M3 -- error
	mqcpxrs.p  fr0,fr2,acc0	; M4
	mqmulhu    fr4,fr6,acc8 ; M4 -- error
	mqcpxrs.p  fr0,fr2,acc0	; M4
	mcuti      acc8,#2,fr8	; M5 -- ok
	mqcpxrs.p  fr0,fr2,acc0	; M4
	mdcutssi   acc8,#2,fr8	; M6 -- ok

	; M-5 first
	mrdacc.p   acc0,fr0	; M5
	mpackh     fr4,fr5,fr6  ; M1 -- ok
	mrdacc.p   acc0,fr0	; M5
	mcpli      fr4,#1,fr6	; M2 -- error
	mrdacc.p   acc0,fr0	; M5
	mmulhu     fr4,fr6,acc8 ; M3 -- ok
	mrdacc.p   acc0,fr0	; M5
	mqmulhu    fr4,fr6,acc8 ; M4 -- error
	mrdacc.p   acc0,fr0	; M5
	mcuti      acc8,#2,fr8	; M5 -- ok
	mrdacc.p   acc0,fr0	; M5
	mdcutssi   acc8,#2,fr8	; M6 -- error

	; M-6 first
	mdcutssi.p acc0,#3,fr0	; M6
	mpackh     fr4,fr5,fr6  ; M1 -- error
	mdcutssi.p acc0,#3,fr0	; M6
	mcpli      fr4,#1,fr6	; M2 -- error
	mdcutssi.p acc0,#3,fr0	; M6
	mmulhu     fr4,fr6,acc8 ; M3 -- error
	mdcutssi.p acc0,#3,fr0	; M6
	mqmulhu    fr4,fr6,acc8 ; M4 -- error
	mdcutssi.p acc0,#3,fr0	; M6
	mcuti      acc8,#2,fr8	; M5 -- error
	mdcutssi.p acc0,#3,fr0	; M6
	mdcutssi   acc8,#2,fr8	; M6 -- error
