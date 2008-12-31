# $FreeBSD: src/sys/boot/efi/Makefile.inc,v 1.7.26.1 2008/11/25 02:59:29 kensmith Exp $

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
