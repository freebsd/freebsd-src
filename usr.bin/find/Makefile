#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	find
SRCS=	find.c function.c ls.c main.c misc.c operator.c option.c \
	getdate.y
CFLAGS+= -I${.CURDIR}
YFLAGS=

.include <bsd.prog.mk>

# XXX Temporary! parsedate is broken!
.PATH:	${.CURDIR}/../../contrib/cvs/lib/
CFLAGS+=-DHAVE_TIMEZONE
