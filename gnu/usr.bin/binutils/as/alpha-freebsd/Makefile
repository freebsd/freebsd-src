# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc0"

.if ${TARGET_ARCH} != "alpha"
NOMAN=	true
BINDIR=	/usr/libexec/cross/alpha-freebsd
.endif
SRCS+=	obj-elf.c tc-alpha.c

.include <bsd.prog.mk>
