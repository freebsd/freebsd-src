# $FreeBSD$

LDFLAGS+=	-nostdlib

# BERI boot loader is built with '-mno-abicalls' flag,
# which is incompatible with PIE
MK_PIE=		no

.include "../Makefile.inc"
