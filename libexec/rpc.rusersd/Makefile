#	$Id: Makefile,v 1.1 1993/09/16 00:34:36 jtc Exp $

PROG =	rpc.rusersd
SRCS =	rusersd.c rusers_proc.c
MAN8 =	rpc.rusersd.8

DPADD=	${LIBRPCSVC} ${LIBRPC} ${LIBUTIL}
LDADD=	-lrpcsvc -lrpc -lutil

.if exists(/usr/include/X11/extensions/xidle.h)
CFLAGS+= -DXIDLE
LDADD+= -L/usr/X386/lib -lXext -lX11
.endif


.include <bsd.prog.mk>
