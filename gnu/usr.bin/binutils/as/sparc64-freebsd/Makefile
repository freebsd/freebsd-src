#
# $FreeBSD$
#

.include "${.CURDIR}/../Makefile.inc0"

PROG=		as
.if	${TARGET_ARCH} != "sparc64"
NOMAN=		1
BINDIR=		/usr/libexec/cross/sparc64-freebsd
.endif
SRCS+=		obj-elf.c tc-sparc.c

.include <bsd.prog.mk>
