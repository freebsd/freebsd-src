# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpdate

PROG=	ntpdate
MAN=
SRCS=	ntpdate.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/include/ \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/unix/include/ \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/pthreads/include \
	-I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBM} ${LIBPTHREAD}
LDADD=	${LIBNTP} -lm -lpthread

.if ${MK_OPENSSL} != "no"
DPADD+=	${LIBCRYPTO}
LDADD+=	-lcrypto
.else
DPADD+=	${LIBMD}
LDADD+=	-lmd
.endif

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpdate

.include <bsd.prog.mk>
