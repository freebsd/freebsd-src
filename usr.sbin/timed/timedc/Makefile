#	@(#)Makefile	5.2 (Berkeley) 5/11/90

PROG=	timedc
SRCS=	cmds.c cmdtab.c timedc.c byteorder.c measure.c cksum.${MACHINE}.c
MAN8=	timedc.8
BINOWN=	root
BINMODE=4555
.PATH:	${.CURDIR}/../timed ${.CURDIR}/../timed/obj

.include "../../Makefile.inc"
.include <bsd.prog.mk>
