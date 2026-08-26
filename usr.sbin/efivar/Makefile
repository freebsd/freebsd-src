PACKAGE=	efi-tools

PROG=	efivar
MAN=	efivar.8

LIBADD= efivar geom

SRCS=	efivar.c

EFIBOOT=${SRCTOP}/stand/efi
CFLAGS+=	-I${EFIBOOT}/include

.include <bsd.prog.mk>
