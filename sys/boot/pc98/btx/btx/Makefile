# $FreeBSD: src/sys/boot/pc98/btx/btx/Makefile,v 1.12.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	btx
INTERNALPROG=
NO_MAN=
SRCS=	btx.S

.if defined(PAGING)
CFLAGS+=-DPAGING
.endif

.if defined(BOOT_BTX_NOHANG)
BOOT_BTX_FLAGS=0x1
.else
BOOT_BTX_FLAGS=0x0
.endif

CFLAGS+=-DBTX_FLAGS=${BOOT_BTX_FLAGS}

.if defined(BTX_SERIAL)
BOOT_COMCONSOLE_PORT?= 0x238
BOOT_COMCONSOLE_SPEED?= 9600
B2SIOFMT?=	0x3

CFLAGS+=-DBTX_SERIAL -DSIOPRT=${BOOT_COMCONSOLE_PORT} \
	-DSIOFMT=${B2SIOFMT} -DSIOSPD=${BOOT_COMCONSOLE_SPEED}
.endif

ORG=	0x9000

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
