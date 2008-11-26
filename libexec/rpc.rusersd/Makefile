# $FreeBSD: src/libexec/rpc.rusersd/Makefile,v 1.8.30.1 2008/10/02 02:57:24 kensmith Exp $

PROG =	rpc.rusersd
SRCS =	rusersd.c rusers_proc.c
MAN =	rpc.rusersd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

#.if exists(/usr/X11R6/include/X11/extensions/xidle.h)
#CFLAGS+= -DXIDLE
#LDADD+= -L/usr/X11R6/lib -lXext -lX11
#.endif


.include <bsd.prog.mk>
