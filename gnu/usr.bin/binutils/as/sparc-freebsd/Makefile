#
# $FreeBSD$
#

.include "${.CURDIR}/../Makefile.inc0"

PROG=		as
.if	${MACHINE_ARCH} != "sparc"
NOMAN=		1
BINDIR=		/usr/libexec/cross/sparc-freebsdelf
.endif
SRCS+=		obj-elf.c tc-sparc.c

.include <bsd.prog.mk>
