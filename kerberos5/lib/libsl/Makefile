# $FreeBSD$

LIB=	sl

SRCS=	\
	sl.c			 	\
	${.OBJDIR}/roken.h

CFLAGS+=-I${KRB5DIR}/lib/sl		\
	-I${KRB5DIR}/lib/roken		\
	-I${KRB5DIR}/include		\
	-I${.OBJDIR}

INTERNALLIB= yes

.include <bsd.lib.mk>

.PATH:	${KRB5DIR}/lib/sl

build-tools:	make-roken
