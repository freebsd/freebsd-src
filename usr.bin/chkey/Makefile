# $FreeBSD$

.include <src.opts.mk>

.PATH: ${.CURDIR}/../newkey

PROG=	chkey
SRCS=	chkey.c generic.c update.c
CFLAGS+= -I${.CURDIR}/../newkey
.if ${MK_NIS} != "no"
CFLAGS+= -DYP
.endif
LIBADD+=	rpcsvc mp

.include <bsd.prog.mk>
