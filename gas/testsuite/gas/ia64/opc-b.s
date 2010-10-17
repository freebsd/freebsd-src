.L0:

	{ .bbb; nop.b 0
(p2)	br.cond.sptk .L1
	br.cond.sptk .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.sptk.clr .L1
	br.cond.sptk.clr .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.sptk.few .L1
	br.cond.sptk.few .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.sptk.few.clr .L1
	br.cond.sptk.few.clr .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.sptk.many .L1
	br.cond.sptk.many .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.sptk.many.clr .L1
	br.cond.sptk.many.clr .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.spnt .L1
	br.cond.spnt .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.spnt.clr .L1
	br.cond.spnt.clr .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.spnt.few .L1
	br.cond.spnt.few .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.spnt.few.clr .L1
	br.cond.spnt.few.clr .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.spnt.many .L1
	br.cond.spnt.many .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.spnt.many.clr .L1
	br.cond.spnt.many.clr .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.dptk .L1
	br.cond.dptk .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.dptk.clr .L1
	br.cond.dptk.clr .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.dptk.few .L1
	br.cond.dptk.few .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.dptk.few.clr .L1
	br.cond.dptk.few.clr .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.dptk.many .L1
	br.cond.dptk.many .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.dptk.many.clr .L1
	br.cond.dptk.many.clr .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.dpnt .L1
	br.cond.dpnt .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.dpnt.clr .L1
	br.cond.dpnt.clr .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.dpnt.few .L1
	br.cond.dpnt.few .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.dpnt.few.clr .L1
	br.cond.dpnt.few.clr .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.dpnt.many .L1
	br.cond.dpnt.many .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.cond.dpnt.many.clr .L1
	br.cond.dpnt.many.clr .L0
	;; }

	{ .bbb; (p2) br.wexit.sptk .L1 ;; }
	{ .bbb; br.wexit.sptk .L1 ;; }
	{ .bbb; (p2) br.wexit.sptk.clr .L1 ;; }
	{ .bbb; br.wexit.sptk.clr .L1 ;; }
	{ .bbb; (p2) br.wexit.sptk.few .L1 ;; }
	{ .bbb; br.wexit.sptk.few .L1 ;; }
	{ .bbb; (p2) br.wexit.sptk.few.clr .L1 ;; }
	{ .bbb; br.wexit.sptk.few.clr .L1 ;; }
	{ .bbb; (p2) br.wexit.sptk.many .L1 ;; }
	{ .bbb; br.wexit.sptk.many .L1 ;; }
	{ .bbb; (p2) br.wexit.sptk.many.clr .L1 ;; }
	{ .bbb; br.wexit.sptk.many.clr .L1 ;; }
	{ .bbb; (p2) br.wexit.spnt .L1 ;; }
	{ .bbb; br.wexit.spnt .L1 ;; }
	{ .bbb; (p2) br.wexit.spnt.clr .L1 ;; }
	{ .bbb; br.wexit.spnt.clr .L1 ;; }
	{ .bbb; (p2) br.wexit.spnt.few .L1 ;; }
	{ .bbb; br.wexit.spnt.few .L1 ;; }
	{ .bbb; (p2) br.wexit.spnt.few.clr .L1 ;; }
	{ .bbb; br.wexit.spnt.few.clr .L1 ;; }
	{ .bbb; (p2) br.wexit.spnt.many .L1 ;; }
	{ .bbb; br.wexit.spnt.many .L1 ;; }
	{ .bbb; (p2) br.wexit.spnt.many.clr .L1 ;; }
	{ .bbb; br.wexit.spnt.many.clr .L1 ;; }
	{ .bbb; (p2) br.wexit.dptk .L1 ;; }
	{ .bbb; br.wexit.dptk .L1 ;; }
	{ .bbb; (p2) br.wexit.dptk.clr .L1 ;; }
	{ .bbb; br.wexit.dptk.clr .L1 ;; }
	{ .bbb; (p2) br.wexit.dptk.few .L1 ;; }
	{ .bbb; br.wexit.dptk.few .L1 ;; }
	{ .bbb; (p2) br.wexit.dptk.few.clr .L1 ;; }
	{ .bbb; br.wexit.dptk.few.clr .L1 ;; }
	{ .bbb; (p2) br.wexit.dptk.many .L1 ;; }
	{ .bbb; br.wexit.dptk.many .L1 ;; }
	{ .bbb; (p2) br.wexit.dptk.many.clr .L1 ;; }
	{ .bbb; br.wexit.dptk.many.clr .L1 ;; }
	{ .bbb; (p2) br.wexit.dpnt .L1 ;; }
	{ .bbb; br.wexit.dpnt .L1 ;; }
	{ .bbb; (p2) br.wexit.dpnt.clr .L1 ;; }
	{ .bbb; br.wexit.dpnt.clr .L1 ;; }
	{ .bbb; (p2) br.wexit.dpnt.few .L1 ;; }
	{ .bbb; br.wexit.dpnt.few .L1 ;; }
	{ .bbb; (p2) br.wexit.dpnt.few.clr .L1 ;; }
	{ .bbb; br.wexit.dpnt.few.clr .L1 ;; }
	{ .bbb; (p2) br.wexit.dpnt.many .L1 ;; }
	{ .bbb; br.wexit.dpnt.many .L1 ;; }
	{ .bbb; (p2) br.wexit.dpnt.many.clr .L1 ;; }
	{ .bbb; br.wexit.dpnt.many.clr .L1 ;; }

	{ .bbb; (p2) br.wtop.sptk .L1 ;; }
	{ .bbb; br.wtop.sptk .L1 ;; }
	{ .bbb; (p2) br.wtop.sptk.clr .L1 ;; }
	{ .bbb; br.wtop.sptk.clr .L1 ;; }
	{ .bbb; (p2) br.wtop.sptk.few .L1 ;; }
	{ .bbb; br.wtop.sptk.few .L1 ;; }
	{ .bbb; (p2) br.wtop.sptk.few.clr .L1 ;; }
	{ .bbb; br.wtop.sptk.few.clr .L1 ;; }
	{ .bbb; (p2) br.wtop.sptk.many .L1 ;; }
	{ .bbb; br.wtop.sptk.many .L1 ;; }
	{ .bbb; (p2) br.wtop.sptk.many.clr .L1 ;; }
	{ .bbb; br.wtop.sptk.many.clr .L1 ;; }
	{ .bbb; (p2) br.wtop.spnt .L1 ;; }
	{ .bbb; br.wtop.spnt .L1 ;; }
	{ .bbb; (p2) br.wtop.spnt.clr .L1 ;; }
	{ .bbb; br.wtop.spnt.clr .L1 ;; }
	{ .bbb; (p2) br.wtop.spnt.few .L1 ;; }
	{ .bbb; br.wtop.spnt.few .L1 ;; }
	{ .bbb; (p2) br.wtop.spnt.few.clr .L1 ;; }
	{ .bbb; br.wtop.spnt.few.clr .L1 ;; }
	{ .bbb; (p2) br.wtop.spnt.many .L1 ;; }
	{ .bbb; br.wtop.spnt.many .L1 ;; }
	{ .bbb; (p2) br.wtop.spnt.many.clr .L1 ;; }
	{ .bbb; br.wtop.spnt.many.clr .L1 ;; }
	{ .bbb; (p2) br.wtop.dptk .L1 ;; }
	{ .bbb; br.wtop.dptk .L1 ;; }
	{ .bbb; (p2) br.wtop.dptk.clr .L1 ;; }
	{ .bbb; br.wtop.dptk.clr .L1 ;; }
	{ .bbb; (p2) br.wtop.dptk.few .L1 ;; }
	{ .bbb; br.wtop.dptk.few .L1 ;; }
	{ .bbb; (p2) br.wtop.dptk.few.clr .L1 ;; }
	{ .bbb; br.wtop.dptk.few.clr .L1 ;; }
	{ .bbb; (p2) br.wtop.dptk.many .L1 ;; }
	{ .bbb; br.wtop.dptk.many .L1 ;; }
	{ .bbb; (p2) br.wtop.dptk.many.clr .L1 ;; }
	{ .bbb; br.wtop.dptk.many.clr .L1 ;; }
	{ .bbb; (p2) br.wtop.dpnt .L1 ;; }
	{ .bbb; br.wtop.dpnt .L1 ;; }
	{ .bbb; (p2) br.wtop.dpnt.clr .L1 ;; }
	{ .bbb; br.wtop.dpnt.clr .L1 ;; }
	{ .bbb; (p2) br.wtop.dpnt.few .L1 ;; }
	{ .bbb; br.wtop.dpnt.few .L1 ;; }
	{ .bbb; (p2) br.wtop.dpnt.few.clr .L1 ;; }
	{ .bbb; br.wtop.dpnt.few.clr .L1 ;; }
	{ .bbb; (p2) br.wtop.dpnt.many .L1 ;; }
	{ .bbb; br.wtop.dpnt.many .L1 ;; }
	{ .bbb; (p2) br.wtop.dpnt.many.clr .L1 ;; }
	{ .bbb; br.wtop.dpnt.many.clr .L1 ;; }

	{ .bbb; br.cloop.sptk .L1 ;; }
	{ .bbb; br.cloop.sptk.clr .L1 ;; }
	{ .bbb; br.cloop.sptk.few .L1 ;; }
	{ .bbb; br.cloop.sptk.few.clr .L1 ;; }
	{ .bbb; br.cloop.sptk.many .L1 ;; }
	{ .bbb; br.cloop.sptk.many.clr .L1 ;; }
	{ .bbb; br.cloop.spnt .L1 ;; }
	{ .bbb; br.cloop.spnt.clr .L1 ;; }
	{ .bbb; br.cloop.spnt.few .L1 ;; }
	{ .bbb; br.cloop.spnt.few.clr .L1 ;; }
	{ .bbb; br.cloop.spnt.many .L1 ;; }
	{ .bbb; br.cloop.spnt.many.clr .L1 ;; }
	{ .bbb; br.cloop.dptk .L1 ;; }
	{ .bbb; br.cloop.dptk.clr .L1 ;; }
	{ .bbb; br.cloop.dptk.few .L1 ;; }
	{ .bbb; br.cloop.dptk.few.clr .L1 ;; }
	{ .bbb; br.cloop.dptk.many .L1 ;; }
	{ .bbb; br.cloop.dptk.many.clr .L1 ;; }
	{ .bbb; br.cloop.dpnt .L1 ;; }
	{ .bbb; br.cloop.dpnt.clr .L1 ;; }
	{ .bbb; br.cloop.dpnt.few .L1 ;; }
	{ .bbb; br.cloop.dpnt.few.clr .L1 ;; }
	{ .bbb; br.cloop.dpnt.many .L1 ;; }
	{ .bbb; br.cloop.dpnt.many.clr .L1 ;; }

	{ .bbb; br.cexit.sptk .L1 ;; }
	{ .bbb; br.cexit.sptk.clr .L1 ;; }
	{ .bbb; br.cexit.sptk.few .L1 ;; }
	{ .bbb; br.cexit.sptk.few.clr .L1 ;; }
	{ .bbb; br.cexit.sptk.many .L1 ;; }
	{ .bbb; br.cexit.sptk.many.clr .L1 ;; }
	{ .bbb; br.cexit.spnt .L1 ;; }
	{ .bbb; br.cexit.spnt.clr .L1 ;; }
	{ .bbb; br.cexit.spnt.few .L1 ;; }
	{ .bbb; br.cexit.spnt.few.clr .L1 ;; }
	{ .bbb; br.cexit.spnt.many .L1 ;; }
	{ .bbb; br.cexit.spnt.many.clr .L1 ;; }
	{ .bbb; br.cexit.dptk .L1 ;; }
	{ .bbb; br.cexit.dptk.clr .L1 ;; }
	{ .bbb; br.cexit.dptk.few .L1 ;; }
	{ .bbb; br.cexit.dptk.few.clr .L1 ;; }
	{ .bbb; br.cexit.dptk.many .L1 ;; }
	{ .bbb; br.cexit.dptk.many.clr .L1 ;; }
	{ .bbb; br.cexit.dpnt .L1 ;; }
	{ .bbb; br.cexit.dpnt.clr .L1 ;; }
	{ .bbb; br.cexit.dpnt.few .L1 ;; }
	{ .bbb; br.cexit.dpnt.few.clr .L1 ;; }
	{ .bbb; br.cexit.dpnt.many .L1 ;; }
	{ .bbb; br.cexit.dpnt.many.clr .L1 ;; }

	{ .bbb; br.ctop.sptk .L1 ;; }
	{ .bbb; br.ctop.sptk.clr .L1 ;; }
	{ .bbb; br.ctop.sptk.few .L1 ;; }
	{ .bbb; br.ctop.sptk.few.clr .L1 ;; }
	{ .bbb; br.ctop.sptk.many .L1 ;; }
	{ .bbb; br.ctop.sptk.many.clr .L1 ;; }
	{ .bbb; br.ctop.spnt .L1 ;; }
	{ .bbb; br.ctop.spnt.clr .L1 ;; }
	{ .bbb; br.ctop.spnt.few .L1 ;; }
	{ .bbb; br.ctop.spnt.few.clr .L1 ;; }
	{ .bbb; br.ctop.spnt.many .L1 ;; }
	{ .bbb; br.ctop.spnt.many.clr .L1 ;; }
	{ .bbb; br.ctop.dptk .L1 ;; }
	{ .bbb; br.ctop.dptk.clr .L1 ;; }
	{ .bbb; br.ctop.dptk.few .L1 ;; }
	{ .bbb; br.ctop.dptk.few.clr .L1 ;; }
	{ .bbb; br.ctop.dptk.many .L1 ;; }
	{ .bbb; br.ctop.dptk.many.clr .L1 ;; }
	{ .bbb; br.ctop.dpnt .L1 ;; }
	{ .bbb; br.ctop.dpnt.clr .L1 ;; }
	{ .bbb; br.ctop.dpnt.few .L1 ;; }
	{ .bbb; br.ctop.dpnt.few.clr .L1 ;; }
	{ .bbb; br.ctop.dpnt.many .L1 ;; }
	{ .bbb; br.ctop.dpnt.many.clr .L1 ;; }

	{ .bbb; nop.b 0
(p2)	br.call.sptk b0 = .L1
	br.call.sptk b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.sptk.clr b0 = .L1
	br.call.sptk.clr b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.sptk.few b0 = .L1
	br.call.sptk.few b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.sptk.few.clr b0 = .L1
	br.call.sptk.few.clr b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.sptk.many b0 = .L1
	br.call.sptk.many b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.sptk.many.clr b0 = .L1
	br.call.sptk.many.clr b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.spnt b0 = .L1
	br.call.spnt b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.spnt.clr b0 = .L1
	br.call.spnt.clr b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.spnt.few b0 = .L1
	br.call.spnt.few b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.spnt.few.clr b0 = .L1
	br.call.spnt.few.clr b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.spnt.many b0 = .L1
	br.call.spnt.many b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.spnt.many.clr b0 = .L1
	br.call.spnt.many.clr b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.dptk b0 = .L1
	br.call.dptk b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.dptk.clr b0 = .L1
	br.call.dptk.clr b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.dptk.few b0 = .L1
	br.call.dptk.few b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.dptk.few.clr b0 = .L1
	br.call.dptk.few.clr b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.dptk.many b0 = .L1
	br.call.dptk.many b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.dptk.many.clr b0 = .L1
	br.call.dptk.many.clr b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.dpnt b0 = .L1
	br.call.dpnt b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.dpnt.clr b0 = .L1
	br.call.dpnt.clr b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.dpnt.few b0 = .L1
	br.call.dpnt.few b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.dpnt.few.clr b0 = .L1
	br.call.dpnt.few.clr b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.dpnt.many b0 = .L1
	br.call.dpnt.many b0 = .L0
	;; }
	{ .bbb; nop.b 0
(p2)	br.call.dpnt.many.clr b0 = .L1
	br.call.dpnt.many.clr b0 = .L0
	;; }

	{ .bbb; nop.b 0;
(p2)	br.cond.sptk b2
	br.cond.sptk b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.sptk.clr b2
	br.cond.sptk.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.sptk.few b2
	br.cond.sptk.few b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.sptk.few.clr b2
	br.cond.sptk.few.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.sptk.many b2
	br.cond.sptk.many b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.sptk.many.clr b2
	br.cond.sptk.many.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.spnt b2
	br.cond.spnt b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.spnt.clr b2
	br.cond.spnt.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.spnt.few b2
	br.cond.spnt.few b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.spnt.few.clr b2
	br.cond.spnt.few.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.spnt.many b2
	br.cond.spnt.many b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.spnt.many.clr b2
	br.cond.spnt.many.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.dptk b2
	br.cond.dptk b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.dptk.clr b2
	br.cond.dptk.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.dptk.few b2
	br.cond.dptk.few b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.dptk.few.clr b2
	br.cond.dptk.few.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.dptk.many b2
	br.cond.dptk.many b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.dptk.many.clr b2
	br.cond.dptk.many.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.dpnt b2
	br.cond.dpnt b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.dpnt.clr b2
	br.cond.dpnt.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.dpnt.few b2
	br.cond.dpnt.few b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.dpnt.few.clr b2
	br.cond.dpnt.few.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.dpnt.many b2
	br.cond.dpnt.many b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.cond.dpnt.many.clr b2
	br.cond.dpnt.many.clr b2
	;; }

	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.sptk b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.sptk.clr b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.sptk.few b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.sptk.few.clr b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.sptk.many b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.sptk.many.clr b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.spnt b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.spnt.clr b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.spnt.few b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.spnt.few.clr b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.spnt.many b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.spnt.many.clr b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.dptk b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.dptk.clr b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.dptk.few b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.dptk.few.clr b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.dptk.many b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.dptk.many.clr b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.dpnt b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.dpnt.clr b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.dpnt.few b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.dpnt.few.clr b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.dpnt.many b2
	;; }
	{ .bbb; nop.b 0;
	nop.b 0
	br.ia.dpnt.many.clr b2
	;; }

	{ .bbb; nop.b 0;
(p2)	br.ret.sptk b2
	br.ret.sptk b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.sptk.clr b2
	br.ret.sptk.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.sptk.few b2
	br.ret.sptk.few b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.sptk.few.clr b2
	br.ret.sptk.few.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.sptk.many b2
	br.ret.sptk.many b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.sptk.many.clr b2
	br.ret.sptk.many.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.spnt b2
	br.ret.spnt b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.spnt.clr b2
	br.ret.spnt.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.spnt.few b2
	br.ret.spnt.few b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.spnt.few.clr b2
	br.ret.spnt.few.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.spnt.many b2
	br.ret.spnt.many b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.spnt.many.clr b2
	br.ret.spnt.many.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.dptk b2
	br.ret.dptk b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.dptk.clr b2
	br.ret.dptk.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.dptk.few b2
	br.ret.dptk.few b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.dptk.few.clr b2
	br.ret.dptk.few.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.dptk.many b2
	br.ret.dptk.many b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.dptk.many.clr b2
	br.ret.dptk.many.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.dpnt b2
	br.ret.dpnt b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.dpnt.clr b2
	br.ret.dpnt.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.dpnt.few b2
	br.ret.dpnt.few b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.dpnt.few.clr b2
	br.ret.dpnt.few.clr b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.dpnt.many b2
	br.ret.dpnt.many b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.ret.dpnt.many.clr b2
	br.ret.dpnt.many.clr b2
	;; }

	{ .bbb; nop.b 0;
(p2)	br.call.sptk b0 = b2
	br.call.sptk b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.sptk.clr b0 = b2
	br.call.sptk.clr b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.sptk.few b0 = b2
	br.call.sptk.few b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.sptk.few.clr b0 = b2
	br.call.sptk.few.clr b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.sptk.many b0 = b2
	br.call.sptk.many b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.sptk.many.clr b0 = b2
	br.call.sptk.many.clr b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.spnt b0 = b2
	br.call.spnt b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.spnt.clr b0 = b2
	br.call.spnt.clr b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.spnt.few b0 = b2
	br.call.spnt.few b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.spnt.few.clr b0 = b2
	br.call.spnt.few.clr b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.spnt.many b0 = b2
	br.call.spnt.many b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.spnt.many.clr b0 = b2
	br.call.spnt.many.clr b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.dptk b0 = b2
	br.call.dptk b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.dptk.clr b0 = b2
	br.call.dptk.clr b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.dptk.few b0 = b2
	br.call.dptk.few b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.dptk.few.clr b0 = b2
	br.call.dptk.few.clr b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.dptk.many b0 = b2
	br.call.dptk.many b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.dptk.many.clr b0 = b2
	br.call.dptk.many.clr b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.dpnt b0 = b2
	br.call.dpnt b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.dpnt.clr b0 = b2
	br.call.dpnt.clr b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.dpnt.few b0 = b2
	br.call.dpnt.few b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.dpnt.few.clr b0 = b2
	br.call.dpnt.few.clr b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.dpnt.many b0 = b2
	br.call.dpnt.many b0 = b2
	;; }
	{ .bbb; nop.b 0;
(p2)	br.call.dpnt.many.clr b0 = b2
	br.call.dpnt.many.clr b0 = b2
	;; }

	{ .bbb; break.b 0; nop.b 0
	brp.sptk .L0, .L2
	;; }
	{ .bbb; break.b 0; nop.b 0
	brp.sptk.imp .L0, .L2
	;; }
.L2:
	{ .bbb; break.b 0; nop.b 0
	brp.loop .L0, .L3
	;; }
	{ .bbb; break.b 0; nop.b 0
	brp.loop.imp .L0, .L3
	;; }
.L3:
	{ .bbb; break.b 0; nop.b 0
	brp.dptk .L0, .L4
	;; }
	{ .bbb; break.b 0; nop.b 0
	brp.dptk.imp .L0, .L4
	;; }
.L4:
	{ .bbb; break.b 0; nop.b 0
	brp.exit .L0, .L5
	;; }
	{ .bbb; break.b 0; nop.b 0
	brp.exit.imp .L0, .L5
	;; }
.L5:

	{ .bbb; break.b 0; nop.b 0
	brp.sptk b3, .L6
	;; }
	{ .bbb; break.b 0; nop.b 0
	brp.sptk.imp b3, .L6
	;; }
.L6:
	{ .bbb; break.b 0; nop.b 0
	brp.dptk b3, .L7
	;; }
	{ .bbb; break.b 0; nop.b 0
	brp.dptk.imp b3, .L7
	;; }
.L7:

	{ .bbb; break.b 0; nop.b 0
	brp.ret.sptk b3, .L8
	;; }
	{ .bbb; break.b 0; nop.b 0
	brp.ret.sptk.imp b3, .L8
	;; }
.L8:
	{ .bbb; break.b 0; nop.b 0
	brp.ret.dptk b3, .L9
	;; }
	{ .bbb; break.b 0; nop.b 0
	brp.ret.dptk.imp b3, .L9
	;; }
.L9:

.space 5888
	{ .bbb; nop.b 0; nop.b 0; cover ;; }
	{ .bbb; nop.b 0; nop.b 0; clrrrb ;; }
	{ .bbb; nop.b 0; nop.b 0; clrrrb.pr ;; }
	{ .bbb; nop.b 0; nop.b 0; rfi ;; }
	{ .bbb; nop.b 0; nop.b 0; bsw.0 ;; }
	{ .bbb; nop.b 0; nop.b 0; bsw.1 ;; }
	{ .bbb; nop.b 0; nop.b 0; epc ;; }

.L1:

	# instructions added by SDM2.1:

	break.b 0x1ffff
	hint.b	@pause
	hint.b	0x1ffff
	nop.b	0x1ffff
