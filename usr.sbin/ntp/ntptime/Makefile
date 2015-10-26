# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/ntp/util

PROG=	ntptime
MAN=

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/include/ \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/unix/include/ \
	-I${.CURDIR}/../../../contrib/ntp/lib/isc/pthreads/include \
	-I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBPTHREAD}
LDADD=	${LIBNTP} -lpthread

.include <bsd.prog.mk>
