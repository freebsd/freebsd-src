# $FreeBSD: src/sys/boot/efi/Makefile.inc,v 1.7.24.1 2008/10/02 02:57:24 kensmith Exp $

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
