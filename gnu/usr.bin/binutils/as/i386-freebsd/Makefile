# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc0"

NOMAN=	true
.if ${TARGET_ARCH} != "i386"
BINDIR=	/usr/libexec/cross/i386-freebsd
.endif
SRCS+=	obj-elf.c tc-i386.c

.include <bsd.prog.mk>
