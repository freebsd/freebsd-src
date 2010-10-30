	.text
	.global iwmmxt2
iwmmxt2:

	waddhc		wr4, wr5, wr6
	waddwc		wr7, wr8, wr9

	wmadduxgt	wr4, wr5, wr6
	wmadduneq	wr7, wr8, wr9
	wmaddsxne	wr4, wr5, wr6
	wmaddsnge	wr7, wr8, wr9

	wmulumr		wr1, wr2, wr3
	wmulsmr		wr1, wr2, wr3

	torvscbgt	r15
	torvschne	r15
	torvscweq	r15

	wabsb		wr1, wr2
	wabsh		wr3, wr4
	wabsw		wr5, wr6
	wabsbgt		wr1, wr2

	wabsdiffb	wr1, wr2, wr3
	wabsdiffh	wr4, wr5, wr6
	wabsdiffw	wr7, wr8, wr9
	wabsdiffbgt	wr1, wr2, wr3

	waddbhusm	wr1, wr2, wr3
	waddbhusl	wr4, wr5, wr6
	waddbhusmgt	wr1, wr2, wr3
	waddbhuslgt	wr4, wr5, wr6

	wavg4		wr1, wr2, wr3
	wavg4gt		wr4, wr5, wr6
	wavg4r		wr1, wr2, wr3
	wavg4rgt	wr4, wr5, wr6

	wldrd		wr1, [r1], -r2
	wldrd		wr2, [r1], -r2,lsl #3
	wldrd		wr3, [r1], +r2
	wldrd		wr4, [r1], +r2,lsl #4
	wldrd		wr5, [r1, -r2]
	wldrd		wr6, [r1, -r2,lsl #3]
	wldrd		wr7, [r1, +r2]
	wldrd		wr8, [r1, +r2,lsl #4]
	wldrd		wr9, [r1, -r2]!
	wldrd		wr10, [r1, -r2,lsl #3]!
	wldrd		wr11, [r1, +r2]!
	wldrd		wr12, [r1, +r2,lsl #4]!

	wmerge		wr1, wr2, wr3, #4
	wmergegt	wr1, wr2, wr3, #4

	wmiatteq	wr1, wr2, wr3
	wmiatbgt	wr1, wr2, wr3
	wmiabtne	wr1, wr2, wr3
	wmiabbgt	wr1, wr2, wr3
	wmiattneq	wr1, wr2, wr3
	wmiatbnne	wr1, wr2, wr3
	wmiabtngt	wr1, wr2, wr3
	wmiabbneq	wr1, wr2, wr3

	wmiawtteq	wr1, wr2, wr3
	wmiawtbgt	wr1, wr2, wr3
	wmiawbtne	wr1, wr2, wr3
	wmiawbbgt	wr1, wr2, wr3
	wmiawttnne	wr1, wr2, wr3
	wmiawtbngt	wr1, wr2, wr3
	wmiawbtneq	wr1, wr2, wr3
	wmiawbbnne	wr1, wr2, wr3

	wmulwumeq	wr1, wr2, wr3
	wmulwumrgt	wr1, wr2, wr3
	wmulwsmne	wr1, wr2, wr3
	wmulwsmreq	wr1, wr2, wr3
	wmulwlgt	wr1, wr2, wr3
	wmulwlge	wr1, wr2, wr3

	wqmiattne	wr1, wr2, wr3
	wqmiattneq	wr1, wr2, wr3
	wqmiatbgt	wr1, wr2, wr3
	wqmiatbnge	wr1, wr2, wr3
	wqmiabtne	wr1, wr2, wr3
	wqmiabtneq	wr1, wr2, wr3
	wqmiabbgt	wr1, wr2, wr3
	wqmiabbnne	wr1, wr2, wr3

	wqmulmgt	wr1, wr2, wr3
	wqmulmreq	wr1, wr2, wr3

	wqmulwmgt	wr1, wr2, wr3
	wqmulwmreq	wr1, wr2, wr3

	wstrd		wr1, [r1], -r2
	wstrd		wr2, [r1], -r2,lsl #3
	wstrd		wr3, [r1], +r2
	wstrd		wr4, [r1], +r2,lsl #4
	wstrd		wr5, [r1, -r2]
	wstrd		wr6, [r1, -r2,lsl #3]
	wstrd		wr7, [r1, +r2]
	wstrd		wr8, [r1, +r2,lsl #4]
	wstrd		wr9, [r1, -r2]!
	wstrd		wr10, [r1, -r2,lsl #3]!
	wstrd		wr11, [r1, +r2]!
	wstrd		wr12, [r1, +r2,lsl #4]!

	wsubaddhxgt	wr1, wr2, wr3

	wrorh		wr1, wr2, #0
	wrorw		wr1, wr2, #0
	wrord		wr1, wr2, #0
	wrorh		wr1, wr2, #21
	wrorw		wr1, wr2, #13
	wrord		wr1, wr2, #14

	wsllh		wr1, wr2, #0
	wsllw		wr1, wr2, #0
	wslld		wr1, wr2, #0
	wsllh		wr2, wr9, #11
	wsllw		wr3, wr5, #13
	wslld		wr3, wr8, #15

	wsrah		wr1, wr2, #0
	wsraw		wr1, wr2, #0
	wsrad		wr1, wr2, #0
	wsrah		wr2, wr9, #12
	wsraw		wr3, wr5, #14
	wsrad		wr3, wr8, #16

	wsrlh		wr1, wr2, #0
	wsrlw		wr1, wr2, #0
	wsrld		wr1, wr2, #0
	wsrlh		wr2, wr9, #12
	wsrlw		wr3, wr5, #14
	wsrld		wr3, wr8, #16
