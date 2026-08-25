EFIBOOT=${SRCTOP}/stand/efi
EFIINCL=${SRCTOP}/stand/efi/include
.PATH: ${EFIBOOT}/libefi
CFLAGS+= -I${EFIINCL}

PACKAGE=	efi-tools

PROG=efibootmgr
MAN=	efibootmgr.8
SRCS= efichar.c efibootmgr.c

LIBADD= efivar geom

.include <bsd.prog.mk>
