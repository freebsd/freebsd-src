#	$NetBSD: Makefile.inc,v 1.27 2005/10/07 17:16:40 tsutsui Exp $
# $FreeBSD$

SRCS+=	infinity.c fabs.c ldexp.c flt_rounds.c

# SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
#	fpsetround.c fpsetsticky.c

SRCS+=	_ctx_start.S _set_tp.c makecontext.c \
	signalcontext.c sigsetjmp.S \
	trivial-getcontextx.c

.if defined(LIBCHERI)
SRCS+=	_setjmp_c.S setjmp_c.S
.else
SRCS+=	_setjmp.S setjmp.S
.endif

.if ${MK_CHERI} == "yes"
# XXX-BD: clang currently miscompiles these.  See CHERI-LLVM bug #125.
#CC.setjmp.S=      ${_CHERI_CC}
#CC._setjmp.S=      ${_CHERI_CC}
.endif
