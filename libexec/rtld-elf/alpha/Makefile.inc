# $FreeBSD: src/libexec/rtld-elf/alpha/Makefile.inc,v 1.1.4.1 2002/09/02 02:10:20 obrien Exp $

CFLAGS+=	-mno-fp-regs
LDFLAGS+=	-Wl,--script,${.CURDIR}/alpha/ld.so.script
