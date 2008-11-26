# $FreeBSD: src/libexec/rtld-elf/alpha/Makefile.inc,v 1.2.26.1 2008/10/02 02:57:24 kensmith Exp $

CFLAGS+=	-mno-fp-regs
LDFLAGS+=	-Wl,--script,${.CURDIR}/alpha/ld.so.script
