# $FreeBSD: src/usr.sbin/ntp/ntp-keygen/Makefile,v 1.2 2004/07/22 09:10:52 roberto Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/util \
       ${.CURDIR}/../../../contrib/ntp/ntpd

PROG=	ntp-keygen
NOMAN=	yes
SRCS=	ntp-keygen.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.if !defined(NOCRYPT) && !defined(NO_OPENSSL) && !defined(RELEASE_CRUNCH)
DPADD+=	${LIBMD} ${LIBCRYPTO}
LDADD+=	-lmd -lcrypto
.endif

.include <bsd.prog.mk>
