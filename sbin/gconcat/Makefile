# $FreeBSD$

PROG=	gconcat
#MAN=	gconcat.8
NOMAN=	not_yet
DPADD=	${LIBGEOM}
LDADD=	-lgeom
WARNS?=	6
CFLAGS+=-I${.CURDIR}/../../sys

.include <bsd.prog.mk>
