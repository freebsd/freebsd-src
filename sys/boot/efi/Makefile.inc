# $FreeBSD: src/sys/boot/efi/Makefile.inc,v 1.7 2004/02/12 08:10:33 ru Exp $

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
