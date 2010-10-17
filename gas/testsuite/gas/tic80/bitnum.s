;; Test that all the predefined symbol names for the BITNUM field
;; are properly accepted and translated to numeric values.  Also
;; verifies that they are disassembled correctly as symbolics, and
;; that the raw numeric values are handled correctly (stored as
;; the one's complement of the operand numeric value.

	bbo	r10,r8,eq.b	; (~0 & 0x1F)
	bbo	r10,r8,ne.b	; (~1 & 0x1F)
	bbo	r10,r8,gt.b	; (~2 & 0x1F)
	bbo	r10,r8,le.b	; (~3 & 0x1F)
	bbo	r10,r8,lt.b	; (~4 & 0x1F)
	bbo	r10,r8,ge.b	; (~5 & 0x1F)
	bbo	r10,r8,hi.b	; (~6 & 0x1F)
	bbo	r10,r8,ls.b	; (~7 & 0x1F)
	bbo	r10,r8,lo.b	; (~8 & 0x1F)
	bbo	r10,r8,hs.b	; (~9 & 0x1F)

	bbo	r10,r8,eq.h	; (~10 & 0x1F)
	bbo	r10,r8,ne.h	; (~11 & 0x1F)
	bbo	r10,r8,gt.h	; (~12 & 0x1F)
	bbo	r10,r8,le.h	; (~13 & 0x1F)
	bbo	r10,r8,lt.h	; (~14 & 0x1F)
	bbo	r10,r8,ge.h	; (~15 & 0x1F)
	bbo	r10,r8,hi.h	; (~16 & 0x1F)
	bbo	r10,r8,ls.h	; (~17 & 0x1F)
	bbo	r10,r8,lo.h	; (~18 & 0x1F)
	bbo	r10,r8,hs.h	; (~19 & 0x1F)

	bbo	r10,r8,eq.w	; (~20 & 0x1F)
	bbo	r10,r8,ne.w	; (~21 & 0x1F)
	bbo	r10,r8,gt.w	; (~22 & 0x1F)
	bbo	r10,r8,le.w	; (~23 & 0x1F)
	bbo	r10,r8,lt.w	; (~24 & 0x1F)
	bbo	r10,r8,ge.w	; (~25 & 0x1F)
	bbo	r10,r8,hi.w	; (~26 & 0x1F)
	bbo	r10,r8,ls.w	; (~27 & 0x1F)
	bbo	r10,r8,lo.w	; (~28 & 0x1F)
	bbo	r10,r8,hs.w	; (~29 & 0x1F)

	bbo	r10,r8,eq.f	; (~20 & 0x1F)
	bbo	r10,r8,ne.f	; (~21 & 0x1F)
	bbo	r10,r8,gt.f	; (~22 & 0x1F)
	bbo	r10,r8,le.f	; (~23 & 0x1F)
	bbo	r10,r8,lt.f	; (~24 & 0x1F)
	bbo	r10,r8,ge.f	; (~25 & 0x1F)
	bbo	r10,r8,ou.f	; (~26 & 0x1F)
	bbo	r10,r8,in.f	; (~27 & 0x1F)
	bbo	r10,r8,ib.f	; (~28 & 0x1F)
	bbo	r10,r8,ob.f	; (~29 & 0x1F)
	bbo	r10,r8,uo.f	; (~30 & 0x1F)
	bbo	r10,r8,or.f	; (~31 & 0x1F)

	bbo	r10,r8,0
	bbo	r10,r8,1
	bbo	r10,r8,2
	bbo	r10,r8,3
	bbo	r10,r8,4
	bbo	r10,r8,5
	bbo	r10,r8,6
	bbo	r10,r8,7
	bbo	r10,r8,8
	bbo	r10,r8,9
	bbo	r10,r8,10
	bbo	r10,r8,11
	bbo	r10,r8,12
	bbo	r10,r8,13
	bbo	r10,r8,14
	bbo	r10,r8,15
	bbo	r10,r8,16
	bbo	r10,r8,17
	bbo	r10,r8,18
	bbo	r10,r8,19
	bbo	r10,r8,20
	bbo	r10,r8,21
	bbo	r10,r8,22
	bbo	r10,r8,23
	bbo	r10,r8,24
	bbo	r10,r8,25
	bbo	r10,r8,26
	bbo	r10,r8,27
	bbo	r10,r8,28
	bbo	r10,r8,29
	bbo	r10,r8,30
	bbo	r10,r8,31

