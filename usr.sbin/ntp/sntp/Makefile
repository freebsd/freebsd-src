# $FreeBSD$

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../../contrib/ntp/sntp

PROG=	sntp
MK_MAN=	no
SRCS= 	crypto.c kod_management.c log.c main.c networking.c \
	sntp-opts.c sntp.c utilities.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include \
	-I${.CURDIR}/../../../contrib/ntp/include \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/include \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/unix/include \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/pthreads/include \
	-I${.CURDIR}/../../../contrib/ntp/sntp \
	-I${.CURDIR}/../../../contrib/ntp/sntp/libopts \
	-I${.CURDIR}/../../../contrib/ntp/sntp/libevent/include \
	-I${.CURDIR}/../libntpevent \
	-I${.CURDIR}/../

DPADD=	${LIBM} ${LIBOPTS} ${LIBNTP} ${LIBNTPEVENT} ${LIBPTHREAD}
LDADD=	-lm ${LIBOPTS} ${LIBNTP} ${LIBNTPEVENT} -lpthread

.if ${MK_OPENSSL} != "no"
DPADD+=	${LIBCRYPTO}
LDADD+=	-lcrypto
.else
DPADD+=	${LIBMD}
LDADD+=	-lmd
.endif

.include <bsd.prog.mk>
