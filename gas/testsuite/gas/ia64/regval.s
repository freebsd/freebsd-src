.explicit
rr1:
	.reg.val r1, 0xE000000000000000
	mov		rr[r0] = r0
	mov		rr[r1] = r0
	br.ret.sptk	rp
	;;
rr2:
	.reg.val r1, 0
	mov		rr[r0] = r0
	mov		rr[r1] = r0
	br.ret.sptk	rp
	;;
rr3:
	movl		r1 = 0xE000000000000000
	;;
	mov		rr[r0] = r0
	mov		rr[r1] = r0
	br.ret.sptk	rp
	;;
rr4:
	mov		r1 = 0
	;;
	mov		rr[r0] = r0
	mov		rr[r1] = r0
	br.ret.sptk	rp
	;;
rr5:
	movl		r1 = xyz+0xE000000000000000
	;;
	mov		rr[r0] = r0
	mov		rr[r1] = r0
	br.ret.sptk	rp
	;;
rr6:
	dep.z		r1 = 1, 61, 3
	;;
	mov		rr[r0] = r0
	mov		rr[r1] = r0
	br.ret.sptk	rp
	;;
rr7:
	dep.z		r1 = -1, 0, 61
	;;
	mov		rr[r0] = r0
	mov		rr[r1] = r0
	br.ret.sptk	rp
	;;
