#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/timed/timedc/Makefile,v 1.7.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../timed

PROG=	timedc
MAN=	timedc.8
SRCS=	cmds.c cmdtab.c timedc.c byteorder.c measure.c cksum.c
BINOWN=	root
BINMODE= 4555

.include "../../Makefile.inc"
.include <bsd.prog.mk>
