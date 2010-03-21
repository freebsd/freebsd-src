# $FreeBSD: src/sys/boot/ia64/Makefile.inc,v 1.3.30.1 2010/02/10 00:26:20 kensmith Exp $

BINDIR?=	/boot

# Options used when building standalone components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
