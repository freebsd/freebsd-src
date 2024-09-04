.PATH: ${SRCTOP}/contrib/ntp/util

PACKAGE=ntp
PROG=	ntptime
MAN=

CFLAGS+= -I${SRCTOP}/contrib/ntp/include \
	-I${SRCTOP}/contrib/ntp/libntp/lib/isc/include/ \
	-I${SRCTOP}/contrib/ntp/libntp/lib/isc/unix/include/ \
	-I${SRCTOP}/contrib/ntp/libntp/lib/isc/pthreads/include \
	-I${.CURDIR:H}

LIBADD=	m ntp pthread

.include <bsd.prog.mk>
