# $FreeBSD$

LIB=	sl
CFLAGS+=-I${KRB5DIR}/lib/sl \
	-I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/include
SRCS=	sl.c

NOPIC=	yes

install:

.include <bsd.lib.mk>

.PATH:	${KRB5DIR}/lib/sl
