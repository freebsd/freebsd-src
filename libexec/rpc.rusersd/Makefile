# $FreeBSD$

PROG =	rpc.rusersd
SRCS =	rusersd.c rusers_proc.c
MAN8 =	rpc.rusersd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.if exists(/usr/include/X11/extensions/xidle.h)
CFLAGS+= -DXIDLE
LDADD+= -L${X11BASE}/lib -lXext -lX11
.endif


.include <bsd.prog.mk>
