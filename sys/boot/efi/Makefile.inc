# $FreeBSD$

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
