	.text
	.global iwmmxt
iwmmxt:
	
	tandcb		r15
	TANDCHLE	r15
	TANDCWge	r15

	TBCSTBlt	wr0, r1
	tbcsth		wr1, r2
	TBCSTWGT	wr2, r3

	textrcb		r15, #7
	textrcheq	r15, #2
	TEXTRCW		r15, #0

	TEXTRMUB	r14, wr3, #6
	textrmsbne	r13, wr4, #5
	textrmUH	r12, wr5, #2
	textrmSh	r11, wr6, #0
	TEXTRMUWcs	r10, wr7, #1
	textrmswhs	r9,  wr8, #0

	TINSRB		wr9,  r8, #4
	tinsrhcc	wr10, r7, #0
	tinsrw		wr11, r6, #1

	tmcrul		wcid, r5
	TMCRR		wr12, r6, r7
	tmialo		wr13, r5, r4
	tmiaphMI	wr14, r3, r2
	
	TMIAbb		wr15, r0, r1
	TMIAbTpl	wr13, r2, r3
	tmiaBtvs	wr1, r4, r5
	tmiaTTvc	wr2, r6, r7

	tmovmskB	r8, wr3
	TMOVMSKHhi	r9, wr4
	tmovmskwls	r10, wr5

	tmrc		r11, wcon
	TMRRCge		r12, r13, wr6
	
	torcb		r15
	torchlt		r15
	TORCW		r15

	waccb		wr7,  wr8
	WACCHlt		wr9,  wr10
	WACCWGT		wr11, wr12

	waddble		wr13, wr14, wr15
	waddBUS		wr0,  wr2,  wr4
	waddbssal	wr6,  wr8,  wr10
	waddH		wr12, wr14, wr15
	WADDHUSLE	wr13, wr12, wr11
	WADDHSSeq	wr10, wr9,  wr8
	WADDWne		wr7,  wr6, wr5
	waddwus		wr4,  wr3, wr2
	waddwsscs	wr1,  wr0, wr15

	waligni		wr3,  wr5, wr7, #5
	WALIGNR0hs	wr9,  wr11, wr13
	walignr1	wr7,  wr6, wr5
	walignr2cc	wr2,  wr4, wr8
	WALIGNR3ul	wr5,  wr9, wr1

	wand		wr3, wr8, wr1
	wandn		wr3, wr2, wr6

	wavg2b		wr7, wr8, wr9
	wavg2hle	wr10, wr11, wr12
	wavg2brge	wr13, wr14, wr15
	wavg2hr		wr0, wr1, wr12

	wcmpeqb		wr13, wr4, wr5
	wcmpeqheq	wr4, wr7, wr0
	wcmpeqWlt	wr6, wr9, wr8

	wcmpgtUbul	wr1, wr2, wr3
	wcmpgtsb	wr4, wr5, wr6
	wcmpgtuhcc	wr7, wr8, wr9
	wcmpgtsh	wr10, wr11, wr13
	wcmpgtuw	wr2, wr4, wr3
	wcmpgtswhi	wr5, wr6, wr3

	wldrb		wr1, [r0, #36]
	wldrheq		wr2, [r1, #24]!
	wldrwne		wr3, [r2], #16
	wldrdvs		wr4, [r3, #-332]
	wldrw		wcssf, [r1, #20]!
	
	wmacu		wr4, wr7, wr9
	wmacscs		wr8, wr10, wr14
	wmacuzal	wr15, wr12, wr11
	wmacsz		wr3, wr8, wr10

	wmaddu		wr12, wr11, wr7
	wmaddsgt	wr5, wr3, wr15

	wmaxubhs	wr3, wr4, wr5
	wmaxsb		wr3, wr4, wr5
	wmaxuhpl	wr3, wr4, wr5
	wmaxshmi	wr3, wr4, wr5
	wmaxuwge	wr3, wr4, wr5
	wmaxswle	wr3, wr4, wr5

	wminubul	wr4, wr12, wr10
	wminsb		wr4, wr12, wr10
	wminuhvc	wr4, wr12, wr10
	wminsh		wr4, wr12, wr10
	wminuw		wr4, wr12, wr10
	wminswcc	wr4, wr12, wr10

	wmoveq		wr3, wr4

	wmulum		wr2, wr1, wr8
	wmulsm		wr2, wr1, wr8
	wmulul		wr2, wr1, wr8
	wmulslle	wr2, wr1, wr8

	woreq		wr11, wr8, wr14

	wpackhuseq	wr0, wr1, wr3
	wpackwus	wr0, wr1, wr3
	wpackdusal	wr0, wr1, wr3
	wpackhsshi	wr0, wr1, wr3
	wpackwss	wr0, wr1, wr3
	wpackdsseq	wr0, wr1, wr3

	wrorh		wr4, wr5, wr6
	wrorwmi		wr4, wr5, wr6
	wrord		wr4, wr5, wr6
	wrorhg		wr9, wr10, wcgr0
	wrorwgge	wr9, wr10, wcgr1
	wrordg		wr9, wr10, wcgr2

	wsadb		wr2, wr0, wr10
	wsadhal		wr2, wr0, wr10
	wsadbz		wr2, wr0, wr10
	wsadhzle	wr2, wr0, wr10

	wshufheq	wr4, wr9, #251

	wsllh		wr2, wr9, wr4
	wsllw		wr2, wr9, wr4
	wslldeq		wr2, wr9, wr4
	wsllhgeq	wr2, wr9, wcgr3
	wsllwgvc	wr2, wr9, wcgr2
	wslldg		wr2, wr9, wcgr1

	wsrah		wr1, wr5, wr7
	wsraw		wr1, wr5, wr7
	wsradeq		wr1, wr5, wr7
	wsrahg		wr1, wr5, wcgr3
	wsrawgmi	wr1, wr5, wcgr0
	wsradg		wr1, wr5, wcgr1

	wsrlh		wr1, wr5, wr7
	wsrlw		wr1, wr5, wr7
	wsrldeq		wr1, wr5, wr7
	wsrlhg		wr1, wr5, wcgr3
	wsrlwgmi	wr1, wr5, wcgr0
	wsrldg		wr1, wr5, wcgr1

	wstrb		wr1, [r1, #0xFF]
	wstrh		wr1, [r1, #-0xFF]!
	wstrw		wr1, [r1], #4
	wstrd		wr1, [r1, #0x3FC]
	wstrw		wcasf, [r1], #300

	wsubbusul	wr1, wr3, wr14
	wsubhus		wr1, wr3, wr14
	wsubwusul	wr1, wr3, wr14
	wsubbssul	wr1, wr3, wr14
	wsubhssul	wr1, wr3, wr14
	wsubwss		wr1, wr3, wr14

	wunpckehub	wr3, wr6
	wunpckehuhmi	wr3, wr6
	wunpckehuw	wr3, wr6
	wunpckehsb	wr3, wr6
	wunpckehsh	wr3, wr6
	wunpckehsweq	wr3, wr6

	wunpckihb	wr5, wr12, wr10
	wunpckihhhi	wr5, wr12, wr10
	wunpckihw	wr5, wr12, wr10

	wunpckelub	wr3, wr5
	wunpckeluhne	wr3, wr5
	wunpckeluw	wr3, wr5
	wunpckelsbgt	wr3, wr5
	wunpckelsh	wr3, wr5
	wunpckelsw	wr3, wr5

	wunpckilb	wr4, wr5, wr10
	wunpckilh	wr4, wr5, wr10
	wunpckilweq	wr4, wr5, wr10

	wxorne		wr3, wr4, wr5

	wzeroge		wr7

	tmcr		wcgr0, r0
	tmrc		r1, wcgr2

	@ a.out-required section size padding
	nop
