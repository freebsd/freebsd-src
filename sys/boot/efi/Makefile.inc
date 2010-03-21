# $FreeBSD: src/sys/boot/efi/Makefile.inc,v 1.7.30.1 2010/02/10 00:26:20 kensmith Exp $

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
