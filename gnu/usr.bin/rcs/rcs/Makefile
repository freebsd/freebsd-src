PROG= rcs
SRCS=	rcs.c 

.if exists(${.CURDIR}/../lib/obj)
LDADD=  -L${.CURDIR}/../lib/obj -lrcs
.else
LDADD=  -L${.CURDIR}/../lib/ -lrcs
.endif

CFLAGS+= -I${.CURDIR}/../lib

MAN1=	rcs.1 rcsintro.1
MAN5=   rcsfile.5

.include <bsd.prog.mk>
