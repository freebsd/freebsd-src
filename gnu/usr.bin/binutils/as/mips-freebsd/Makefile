#
# $FreeBSD$
#

.include "${.CURDIR}/../Makefile.inc0"

PROG=		as
.if	${MACHINE_ARCH} != "mipsel" && ${MACHINE_ARCH} != "mipseb"
NOMAN=		1
BINDIR=		/usr/libexec/cross/mips-freebsd
# If we aren't doing cross-assembling, some operations can be optimized, since
# byte orders and value sizes don't need to be adjusted.
CFLAGS+=	-DCROSS_COMPILE=1
.endif
.if ${MACHINE_ARCH} == "mipsel"
CFLAGS+=	-DTARGET_BYTES_BIG_ENDIAN=0
.endif

SRCS+=		obj-elf.c tc-mips.c itbl-ops.c itbl-parse.y itbl-lex.l

.include <bsd.prog.mk>
