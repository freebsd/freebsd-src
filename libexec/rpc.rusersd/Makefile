#	$Id: Makefile,v 1.1.1.1.8.1 1996/11/06 08:44:39 phk Exp $

PROG =	rpc.rusersd
SRCS =	rusersd.c rusers_proc.c
MAN8 =	rpc.rusersd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

#.if exists(/usr/X11R6/include/X11/extensions/xidle.h)
#CFLAGS+= -DXIDLE
#LDADD+= -L/usr/X11R6/lib -lXext -lX11
#.endif


.include <bsd.prog.mk>
