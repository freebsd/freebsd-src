#	@(#)Makefile	8.2 (Berkeley) 3/17/94
# $FreeBSD: src/sbin/disklabel/Makefile,v 1.3.14.2 2001/03/04 13:22:39 obrien Exp $

PROG=	disklabel
SRCS=	disklabel.c
MAN8=	disklabel.8
MAN5=	disklabel.5

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
