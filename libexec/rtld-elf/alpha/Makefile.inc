# $FreeBSD$

CFLAGS+=	-mno-fp-regs
LDFLAGS+=	-Wl,--script,${.CURDIR}/alpha/ld.so.script
