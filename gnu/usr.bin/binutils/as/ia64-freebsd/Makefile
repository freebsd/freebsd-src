# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc0"

.if ${TARGET_ARCH} != "ia64"
NOMAN=	 true
BINDIR=	/usr/libexec/cross/ia64-freebsd
.endif
SRCS+=	obj-elf.c tc-ia64.c

.include <bsd.prog.mk>
