# $FreeBSD: src/sys/boot/ia64/Makefile.inc,v 1.3.32.1 2010/12/21 17:10:29 kensmith Exp $

BINDIR?=	/boot

# Options used when building standalone components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
