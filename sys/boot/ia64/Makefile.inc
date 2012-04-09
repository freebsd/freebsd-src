# $FreeBSD: src/sys/boot/ia64/Makefile.inc,v 1.4.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

BINDIR?=	/boot

# Options used when building standalone components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
