# $FreeBSD: src/sys/boot/ia64/Makefile.inc,v 1.3 2004/02/12 08:10:33 ru Exp $

BINDIR?=	/boot

# Options used when building standalone components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
