.include <src.opts.mk>

.PATH:	${SRCTOP}/contrib/ntp/sntp

PACKAGE=ntp
PROG=	sntp
MAN=
SRCS= 	crypto.c kod_management.c log.c main.c networking.c \
	sntp-opts.c sntp.c utilities.c

CFLAGS+= -I${SRCTOP}/contrib/ntp/include \
	-I${SRCTOP}/contrib/ntp/include \
	-I${SRCTOP}/contrib/ntp/libntp/lib/isc/include \
	-I${SRCTOP}/contrib/ntp/libntp/lib/isc/unix/include \
	-I${SRCTOP}/contrib/ntp/libntp/lib/isc/pthreads/include \
	-I${SRCTOP}/contrib/ntp/sntp \
	-I${SRCTOP}/contrib/ntp/sntp/libopts \
	-I${SRCTOP}/contrib/ntp/sntp/libevent/include \
	-I${.CURDIR:H}/libntpevent \
	-I${.CURDIR:H}

LIBADD=	m opts md ntp ntpevent pthread

.if ${MK_OPENSSL} != "no"
LIBADD+=	crypto
.endif

.include <bsd.prog.mk>
