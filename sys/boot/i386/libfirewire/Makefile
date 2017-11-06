# $FreeBSD$

.include <bsd.init.mk>

LIB=		firewire
INTERNALLIB=

.PATH:		${SYSDIR}/dev/dcons ${SYSDIR}/dev/firewire
SRCS+=		firewire.c fwohci.c dconsole.c
SRCS+=		dcons.c fwcrom.c

CFLAGS+=	-D_BOOT

CFLAGS+=	-I${LDRSRC} -I${SYSDIR} -I.
CFLAGS+=	-I${BTXLIB}
CFLAGS+=	-I${BOOTSRC}/i386/libi386

CFLAGS+=	-Wformat -Wall

.include <bsd.lib.mk>
