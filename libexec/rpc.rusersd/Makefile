# $FreeBSD: src/libexec/rpc.rusersd/Makefile,v 1.6.2.1 2001/04/25 10:40:57 ru Exp $

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
