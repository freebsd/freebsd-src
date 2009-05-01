# $FreeBSD: src/sys/boot/ia64/Makefile.inc,v 1.3.28.1 2009/04/15 03:14:26 kensmith Exp $

BINDIR?=	/boot

# Options used when building standalone components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
