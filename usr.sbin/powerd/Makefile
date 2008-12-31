# $FreeBSD: src/usr.sbin/powerd/Makefile,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	powerd
MAN=	powerd.8
WARNS?=	6

DPADD=	${LIBUTIL}
LDADD=	-lutil

.if ${MACHINE_ARCH} == "i386"
CFLAGS+=-DUSE_APM
.endif

.include <bsd.prog.mk>
