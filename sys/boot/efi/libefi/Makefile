# $FreeBSD$

LIB=	efi
INTERNALLIB=

SRCS=	delay.c efi_console.c efinet.c efipart.c errno.c handles.c \
	libefi.c time.c

.if ${MACHINE_ARCH} == "amd64"
CFLAGS+= -fPIC
.endif
CFLAGS+= -I${.CURDIR}/../include
CFLAGS+= -I${.CURDIR}/../include/${MACHINE_CPUARCH}
CFLAGS+= -I${.CURDIR}/../../../../lib/libstand

# Pick up the bootstrap header for some interface items
CFLAGS+= -I${.CURDIR}/../../common

 
# Suppress warning from clang for FreeBSD %b and %D formats
CFLAGS+= -fformat-extensions

.include <bsd.lib.mk>
