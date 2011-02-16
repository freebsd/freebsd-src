# $FreeBSD: src/usr.sbin/powerd/Makefile,v 1.5.14.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	powerd
MAN=	powerd.8
WARNS?=	6

DPADD=	${LIBUTIL}
LDADD=	-lutil

.if ${MACHINE_ARCH} == "i386"
CFLAGS+=-DUSE_APM
.endif

.include <bsd.prog.mk>
