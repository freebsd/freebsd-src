# $FreeBSD$

.PATH:	${SRCTOP}/sys/crypto/openssl
.PATH:	${SRCTOP}/sys/crypto/openssl/${MACHINE_CPUARCH}

KMOD=	ossl
SRCS=	bus_if.h \
	cryptodev_if.h \
	device_if.h \
	ossl.c \
	ossl_sha1.c \
	ossl_sha256.c \
	ossl_sha512.c \
	${SRCS.${MACHINE_CPUARCH}}

SRCS.aarch64= \
	sha1-armv8.S \
	sha256-armv8.S \
	sha512-armv8.S \
	ossl_aarch64.c

SRCS.amd64= \
	sha1-x86_64.S \
	sha256-x86_64.S \
	sha512-x86_64.S \
	ossl_x86.c

SRCS.i386= \
	sha1-586.S \
	sha256-586.S \
	sha512-586.S \
	ossl_x86.c

# For arm64, we are forced to rewrite the compiler invocation for the assembly
# files, to remove -mgeneral-regs-only.
${SRCS.aarch64:M*.S:S/S/o/}: ${.TARGET:R}.S
	${CC} -c ${CFLAGS:N-mgeneral-regs-only} ${WERROR} ${PROF} ${.IMPSRC}
	${CTFCONVERT_CMD}

.include <bsd.kmod.mk>
