#
# $FreeBSD$
#

.include "${.CURDIR}/../Makefile.inc0"

PROG=		as
.if	${TARGET_ARCH} != "alpha"
NOMAN=		1
BINDIR=		/usr/libexec/cross/alpha-freebsd
.endif
SRCS+=		obj-elf.c tc-alpha.c

.include <bsd.prog.mk>
