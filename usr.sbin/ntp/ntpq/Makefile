# $FreeBSD$

MAN=

.include <bsd.own.mk>
.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpq

BINDIR=	/usr/bin

PROG=	ntpq
SRCS= 	ntpq.c ntpq-opts.c ntpq-subs.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include \
	-I${.CURDIR}/../../../contrib/ntp/include \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/include \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/unix/include \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/pthreads/include \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/${NTP_ATOMIC}/include \
	-I${.CURDIR}/../../../contrib/ntp/sntp/libopts \
	-I${.CURDIR}/../

DPADD=	${LIBEDIT} ${LIBNTP} ${LIBOPTS} ${LIBM} ${LIBPTHREAD}
LDADD=	-ledit ${LIBNTP} ${LIBOPTS} -lm -lpthread

.if ${MK_OPENSSL} != "no"
DPADD+=	${LIBCRYPTO}
LDADD+=	-lcrypto
.else
DPADD+=	${LIBMD}
LDADD+=	-lmd
.endif

CFLAGS+=	-DHAVE_LIBEDIT -DHAVE_READLINE_READLINE_H \
		-I${DESTDIR}/${INCLUDEDIR}/edit

CLEANFILES+= .version version.c

version.c:
	sh -e ${.CURDIR}/../scripts/mkver ntpq

.include <bsd.prog.mk>
