# $FreeBSD: src/usr.sbin/powerd/Makefile,v 1.5.12.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	powerd
MAN=	powerd.8
WARNS?=	6

DPADD=	${LIBUTIL}
LDADD=	-lutil

.if ${MACHINE_ARCH} == "i386"
CFLAGS+=-DUSE_APM
.endif

.include <bsd.prog.mk>
