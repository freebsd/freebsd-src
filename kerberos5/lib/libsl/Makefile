# $FreeBSD$

LIB=	sl
CFLAGS+=-I${KRB5DIR}/lib/sl \
	-I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/include \
	-I${.OBJDIR}
SRCS=	sl.c roken.h

INTERNALLIB= yes

.include <bsd.lib.mk>

.PATH:	${KRB5DIR}/lib/sl

build-tools:	make-roken
