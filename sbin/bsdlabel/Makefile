#	@(#)Makefile	8.2 (Berkeley) 3/17/94
# $FreeBSD$

PROG=	disklabel
SRCS=	disklabel.c
MAN=	disklabel.5 disklabel.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif
WARNS=	2

.include <bsd.prog.mk>
