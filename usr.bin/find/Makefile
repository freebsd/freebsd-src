#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

CFLAGS+=-Wall
PROG=	find
SRCS=	find.c function.c ls.c main.c misc.c operator.c option.c getdate.y
CLEANFILES+= getdate.c y.tab.h
CFLAGS+= -I${.CURDIR}/../../gnu/usr.bin/cvs/lib -DHAVE_CONFIG_H
.PATH:	${.CURDIR}/../../contrib/cvs/lib

.include <bsd.prog.mk>
