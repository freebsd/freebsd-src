#
# $FreeBSD$
#

.include "${.CURDIR}/../Makefile.inc0"

PROG=		as
.if	${MACHINE_ARCH} != "i386"
NOMAN=		1
BINDIR=		/usr/libexec/cross/i386-freebsd
.endif
SRCS+=		obj-elf.c tc-i386.c

.include <bsd.prog.mk>
