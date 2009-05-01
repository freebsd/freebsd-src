# $FreeBSD: src/sys/boot/efi/Makefile.inc,v 1.7.28.1 2009/04/15 03:14:26 kensmith Exp $

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
