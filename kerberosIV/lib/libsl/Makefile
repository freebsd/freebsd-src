# $FreeBSD$

LIB=		sl
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/sl \
		-I${KRB4DIR}/lib/roken \
		-I${INCLUDEOBJDIR}
SRCS=		sl.c ss.c strtok_r.c
NOPIC=		yes

install:

.include <bsd.lib.mk>

.PATH:		${KRB4DIR}/lib/sl
.PATH:		${KRB4DIR}/lib/roken
