# $FreeBSD$

CFLAGS+=	-ffreestanding
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
