# $FreeBSD: src/usr.sbin/powerd/Makefile,v 1.5.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	powerd
MAN=	powerd.8
WARNS?=	6

DPADD=	${LIBUTIL}
LDADD=	-lutil

.if ${MACHINE_ARCH} == "i386"
CFLAGS+=-DUSE_APM
.endif

.include <bsd.prog.mk>
