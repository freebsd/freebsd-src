#
# $Id: Makefile,v 1.1 1999/03/01 04:01:56 imp Exp $
#

.include "${.CURDIR}/../Makefile.inc0"

.if	${MACHINE_ARCH} == "mipsel" || ${MACHINE_ARCH} == "mipseb"
PROG=		as
.else
PROG=		as_mips
.endif
.if ${MACHINE_ARCH} == "mipsel"
CFLAGS+=	-DTARGET_BYTES_BIG_ENDIAN=0
.endif

SRCS+=		obj-elf.c tc-mips.c itbl-ops.c itbl-parse.y itbl-lex.l

.include <bsd.prog.mk>
