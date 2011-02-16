# $FreeBSD: src/sys/boot/efi/Makefile.inc,v 1.7.32.1 2010/12/21 17:10:29 kensmith Exp $

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
