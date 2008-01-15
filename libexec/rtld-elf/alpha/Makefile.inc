# $FreeBSD: src/libexec/rtld-elf/alpha/Makefile.inc,v 1.2 2002/02/18 02:24:09 peter Exp $

CFLAGS+=	-mno-fp-regs
LDFLAGS+=	-Wl,--script,${.CURDIR}/alpha/ld.so.script
