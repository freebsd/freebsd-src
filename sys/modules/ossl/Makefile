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

SRCS.amd64= \
	sha1-x86_64.S \
	sha256-x86_64.S \
	sha512-x86_64.S

SRCS.i386= \
	sha1-586.S \
	sha256-586.S \
	sha512-586.S

.include <bsd.kmod.mk>
