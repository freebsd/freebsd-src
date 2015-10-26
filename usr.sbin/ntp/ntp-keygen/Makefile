# $FreeBSD$

NO_MAN=

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../../contrib/ntp/util \
	${.CURDIR}/../../../contrib/ntp/ntpd

PROG=	ntp-keygen
SRCS=	ntp-keygen.c ntp-keygen-opts.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include \
	-I${.CURDIR}/../../../contrib/ntp/include \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/include \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/unix/include \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/pthreads/include \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/${NTP_ATOMIC}/include \
	-I${.CURDIR}/../../../contrib/ntp/sntp/libopts \
	-I${.CURDIR}/../../../lib/libc/${MACHINE_ARCH} \
	-I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBOPTS}
LDADD=	${LIBNTP} ${LIBOPTS}

.if ${MK_OPENSSL} != "no"
DPADD+=	${LIBCRYPTO}
LDADD+=	-lcrypto
.else
DPADD+=	${LIBMD}
LDADD+=	-lmd
.endif

.include <bsd.prog.mk>
