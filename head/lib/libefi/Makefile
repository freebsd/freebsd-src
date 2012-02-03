# $FreeBSD$

.include <bsd.own.mk>

LIB=	efi
SHLIB_MAJOR= 1

SRCS=	libefi.c \
	efi_getvar.c \
	efi_nextvarname.c \
	efi_setvar.c

CFLAGS+= -I${.CURDIR}

INCS=	libefi.h

MAN+=	libefi.3
MLINKS+=libefi.3 efi_getvar.3 \
	libefi.3 efi_nextvarname.3 \
	libefi.3 efi_setvar.3

.include <bsd.lib.mk>
