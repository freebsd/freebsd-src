#
# $FreeBSD$
#

.include "${.CURDIR}/../Makefile.inc0"

PROG=		as
.if	${MACHINE_ARCH} != "alpha"
NOMAN=		1
BINDIR=		/usr/libexec/cross/alpha-freebsdelf
.endif
SRCS+=		obj-elf.c tc-alpha.c

.include <bsd.prog.mk>
