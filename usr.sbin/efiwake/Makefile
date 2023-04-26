# $FreeBSD$

PACKAGE=	efi-tools

PROG=	efiwake
MAN=

SRCS=	efiwake.c

EFIBOOT=${SRCTOP}/stand/efi
CFLAGS+=-I${EFIBOOT}/include

.include <bsd.prog.mk>
