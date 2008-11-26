# $FreeBSD: src/sys/boot/ia64/Makefile.inc,v 1.3.24.1 2008/10/02 02:57:24 kensmith Exp $

BINDIR?=	/boot

# Options used when building standalone components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
