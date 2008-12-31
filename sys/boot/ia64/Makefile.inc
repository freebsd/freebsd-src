# $FreeBSD: src/sys/boot/ia64/Makefile.inc,v 1.3.26.1 2008/11/25 02:59:29 kensmith Exp $

BINDIR?=	/boot

# Options used when building standalone components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
