PROG= ident
SRCS=	ident.c 

.if exists(${.CURDIR}/../lib/obj)
LDADD=  -L${.CURDIR}/../lib/obj -lrcs
.else
LDADD=  -L${.CURDIR}/../lib/ -lrcs
.endif

CFLAGS+= -I${.CURDIR}/../lib

.include <bsd.prog.mk>
