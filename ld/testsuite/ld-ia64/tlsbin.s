	.section ".tbss", "awT", @nobits
	.globl bg1, bg2, bg3, bg4, bg5, bg6, bg7, bg8
bg1:	.space 4
bg2:	.space 4
bg3:	.space 4
bg4:	.space 4
bg5:	.space 4
bg6:	.space 4
bg7:	.space 4
bg8:	.space 4
bl1:	.space 4
bl2:	.space 4
bl3:	.space 4
bl4:	.space 4
bl5:	.space 4
bl6:	.space 4
bl7:	.space 4
bl8:	.space 4
	.explicit
	.pred.safe_across_calls p1-p5,p16-p63
	.text
	.globl	_start#
	.proc	_start#
_start:
	/* IE */
	addl	r14 = @ltoff(@tprel(sG2#)), gp
	;;
	ld8	r14 = [r14]
	;;
	add	r14 = r14, r13
	;;

	/* IE against global symbol in exec */
	addl	r14 = @ltoff(@tprel(bl1#)), gp
	;;
	ld8	r14 = [r14]
	;;
	add	r14 = r14, r13
	;;

	/* LE */
	mov	r2 = r13
	;;
	addl	r14 = @tprel(sg1#), r2
	addl	r15 = @tprel(bl2#) + 2, r2
	;;
	adds	r14 = @tprel(sh2#) + 3, r13
	movl	r15 = @tprel(bl2#) + 1
	;;
	add	r15 = r15, r13
	;;

	br.ret.sptk.many b0
	.endp	_start#
