.explicit
.proc	_start
_start:
	.prologue
{.mii
	nop.m	0
	;;
	.save		ar.lc, r31
	mov		r31 = ar.lc
}	;;
	.body
{.mfb
	br.ret.sptk	rp
}	;;
.endp	_start
