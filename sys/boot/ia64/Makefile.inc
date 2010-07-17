# $FreeBSD: src/sys/boot/ia64/Makefile.inc,v 1.4.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

BINDIR?=	/boot

# Options used when building standalone components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
