#
# $Id: Makefile,v 1.3 1998/05/05 10:31:52 jb Exp $
#

.include "${.CURDIR}/../Makefile.inc0"

.if	${MACHINE_ARCH} == "mipsel" || ${MACHINE_ARCH} == "mipseb"
PROG=		as
.else
PROG=		as_mips
.endif
SRCS+=		obj-elf.c tc-mips.c itbl-ops.c itbl-parse.y itbl-lex.l

.include <bsd.prog.mk>
