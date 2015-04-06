# $FreeBSD$

PROG=	expr
SRCS=	expr.y
YFLAGS=

# expr relies on signed integer wrapping
CFLAGS+= -fwrapv

.include <bsd.prog.mk>
