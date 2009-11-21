# $FreeBSD: src/sys/boot/ia64/Makefile.inc,v 1.4.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

BINDIR?=	/boot

# Options used when building standalone components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
