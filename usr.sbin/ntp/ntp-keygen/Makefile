# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/ntp/util \
       ${.CURDIR}/../../../contrib/ntp/ntpd

PROG=	ntp-keygen
NOMAN=	yes
SRCS=	ntp-keygen.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBMD} ${LIBCRYPTO}
LDADD=	${LIBNTP} -lmd -lcrypto

.include <bsd.prog.mk>
