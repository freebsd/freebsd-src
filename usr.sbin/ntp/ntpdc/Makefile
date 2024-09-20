MAN=

.include <src.opts.mk>

.PATH: ${SRCTOP}/contrib/ntp/ntpdc

PACKAGE=ntp
PROG=	ntpdc
SRCS= 	ntpdc.c ntpdc_ops.c ntpdc-opts.c version.c

CFLAGS+= -I${SRCTOP}/contrib/ntp/include \
	-I${SRCTOP}/contrib/ntp/libntp/lib/isc/include \
	-I${SRCTOP}/contrib/ntp/libntp/lib/isc/unix/include \
	-I${SRCTOP}/contrib/ntp/libntp/lib/isc/pthreads/include \
	-I${SRCTOP}/contrib/ntp/sntp/libopts \
	-I${.CURDIR}/lib/libc/${MACHINE_ARCH} \
	-I${.CURDIR:H} -I${.CURDIR}

LIBADD=	edit ntp m md opts pthread

.if ${MK_OPENSSL} != "no"
LIBADD+=	crypto
.endif

CLEANFILES+= .version version.c

version.c:
	sh -e ${.CURDIR:H}/scripts/mkver ntpdc

.include <bsd.prog.mk>
