#	@(#)Makefile	8.2 (Berkeley) 3/17/94
# $FreeBSD: src/sbin/disklabel/Makefile,v 1.10 2002/04/08 06:26:46 phk Exp $

PROG=	disklabel
SRCS=	disklabel.c
MAN=	disklabel.5 disklabel.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
