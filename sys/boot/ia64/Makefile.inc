# $FreeBSD: src/sys/boot/ia64/Makefile.inc,v 1.4.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

BINDIR?=	/boot

# Options used when building standalone components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
