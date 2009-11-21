# $FreeBSD: src/usr.sbin/powerd/Makefile,v 1.5.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	powerd
MAN=	powerd.8
WARNS?=	6

DPADD=	${LIBUTIL}
LDADD=	-lutil

.if ${MACHINE_ARCH} == "i386"
CFLAGS+=-DUSE_APM
.endif

.include <bsd.prog.mk>
