# $FreeBSD: src/usr.sbin/ntp/ntp-keygen/Makefile,v 1.2.2.1 2005/02/13 07:23:43 obrien Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/util \
       ${.CURDIR}/../../../contrib/ntp/ntpd

PROG=	ntp-keygen
NOMAN=
SRCS=	ntp-keygen.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.if !defined(NOCRYPT) && !defined(NO_OPENSSL) && !defined(RELEASE_CRUNCH)
DPADD+=	${LIBMD} ${LIBCRYPTO}
LDADD+=	-lmd -lcrypto
.endif

.include <bsd.prog.mk>
