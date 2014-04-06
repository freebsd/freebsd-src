# Common defines for all of /sys/boot/amd64/
#
# $FreeBSD$

BINDIR?=	/boot

CFLAGS+=	-ffreestanding
CFLAGS+=	-mno-mmx -mno-3dnow -mno-sse -mno-sse2 -mno-sse3 -msoft-float
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
