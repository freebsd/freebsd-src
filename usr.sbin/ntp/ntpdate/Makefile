.include <src.opts.mk>

.PATH: ${SRCTOP}/contrib/ntp/ntpdate

PACKAGE=ntp
PROG=	ntpdate
MAN=
SRCS=	ntpdate.c version.c

CFLAGS+= -I${SRCTOP}/contrib/ntp/include \
	-I${SRCTOP}/contrib/ntp/libntp/lib/isc/include/ \
	-I${SRCTOP}/contrib/ntp/libntp/lib/isc/unix/include/ \
	-I${SRCTOP}/contrib/ntp/libntp/lib/isc/pthreads/include \
	-I${.CURDIR:H}

LIBADD=	ntp m md pthread

.if ${MK_OPENSSL} != "no"
LIBADD+=	crypto
.endif

CLEANFILES+= .version version.c

version.c:
	sh -e ${.CURDIR:H}/scripts/mkver ntpdate

.include <bsd.prog.mk>
