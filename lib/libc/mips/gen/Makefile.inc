#	$OpenBSD: Makefile.inc,v 1.4 1995/12/15 01:12:36 jonathan Exp $

SRCS+=	_setjmp.S fabs.S frexp.c infinity.c isinf.S ldexp.S modf.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	setjmp.S sigsetjmp.S
