# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/ntp/util \
       ${.CURDIR}/../../../contrib/ntp/ntpd

PROG=	ntp-keygen
NO_MAN=

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.if !defined(NO_CRYPT) && !defined(NO_OPENSSL) && !defined(RELEASE_CRUNCH)
DPADD+=	${LIBMD} ${LIBCRYPTO}
LDADD+=	-lmd -lcrypto
.endif

.include <bsd.prog.mk>
