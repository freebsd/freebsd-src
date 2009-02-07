# $FreeBSD$

PROG=	jexec
MAN=	jexec.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
WARNS?=	6

CFLAGS+= -DSUPPORT_OLD_XPRISON

.include <bsd.prog.mk>
