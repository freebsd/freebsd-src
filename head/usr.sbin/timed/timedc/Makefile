#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.PATH: ${.CURDIR}/../timed

PROG=	timedc
MAN=	timedc.8
SRCS=	cmds.c cmdtab.c timedc.c byteorder.c measure.c cksum.c
BINOWN=	root
BINMODE= 4555

WARNS?=	1

.include "../../Makefile.inc"
.include <bsd.prog.mk>
