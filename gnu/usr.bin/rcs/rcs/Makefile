PROG= rcs

SRCS=	rcs.c 
LDADD=	-L${.CURDIR}/../lib/${.CURDIR} -lrcs
CFLAGS+= -I${.CURDIR}/../lib

MAN1=	rcs.1 rcsintro.1
MAN5=   rcsfile.5

.include <bsd.prog.mk>
