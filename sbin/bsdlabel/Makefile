#	@(#)Makefile	8.2 (Berkeley) 3/17/94
# $FreeBSD$

PROG=	disklabel
SRCS=	disklabel.c 
MAN8=	disklabel.8
MAN5=	disklabel.5

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
