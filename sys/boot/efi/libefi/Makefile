# $FreeBSD$

LIB=	efi
INTERNALLIB=
WARNS?=	2

SRCS=	delay.c efi_console.c efinet.c efipart.c errno.c handles.c \
	libefi.c time.c

.if ${MACHINE_ARCH} == "amd64"
CFLAGS+= -fPIC -mno-red-zone
.endif
CFLAGS+= -I${.CURDIR}/../include
CFLAGS+= -I${.CURDIR}/../include/${MACHINE}
CFLAGS+= -I${.CURDIR}/../../../../lib/libstand

# Pick up the bootstrap header for some interface items
CFLAGS+= -I${.CURDIR}/../../common

 
# Suppress warning from clang for FreeBSD %b and %D formats
CFLAGS+= -fformat-extensions
CFLAGS+= -DTERM_EMU

.include <bsd.lib.mk>
