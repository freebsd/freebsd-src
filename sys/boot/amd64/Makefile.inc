# Common defines for all of /sys/boot/amd64/
#
# $FreeBSD$

BINDIR?=	/boot

# See conf/kern.mk for the correct set of these
CFLAGS+=	-ffreestanding -mno-red-zone
CFLAGS+=	-mno-mmx -mno-sse -mno-aes -mno-avx -msoft-float
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
