# $FreeBSD$

PROG=	efivar
MAN=	efivar.8

LIBADD= efivar

SRCS=	efivar.c efiutil.c

EFIBOOT=${SRCTOP}/stand/efi
CFLAGS+=	-I${EFIBOOT}/include

.include <bsd.prog.mk>
