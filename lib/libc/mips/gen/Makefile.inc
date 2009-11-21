#	$NetBSD: Makefile.inc,v 1.27 2005/10/07 17:16:40 tsutsui Exp $
# $FreeBSD: src/lib/libc/mips/gen/Makefile.inc,v 1.3.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

SRCS+=	infinity.c fabs.c ldexp.c modf.c

# SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
#	fpsetround.c fpsetsticky.c

SRCS+=	_set_tp.c _setjmp.S makecontext.c setjmp.S signalcontext.c sigsetjmp.S
