# $FreeBSD: src/usr.sbin/ntp/ntp-keygen/Makefile,v 1.7.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

NO_MAN=

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../../contrib/ntp/util \
       ${.CURDIR}/../../../contrib/ntp/ntpd

PROG=	ntp-keygen
SRCS=	ntp-keygen.c ntp-keygen-opts.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
	-I${.CURDIR}/../../../contrib/ntp/libopts

DPADD=	${LIBNTP} ${LIBOPTS}
LDADD=	${LIBNTP} ${LIBOPTS}

.if ${MK_OPENSSL} != "no" && !defined(RELEASE_CRUNCH)
DPADD+=	${LIBMD} ${LIBCRYPTO}
LDADD+=	-lmd -lcrypto
.endif

.include <bsd.prog.mk>
